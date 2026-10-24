using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

// A suspended worker is assigned before it can spawn children. Job accounting
// cross-checks notifications: incomplete observation fails closed.
public sealed class SanitizerProcessMonitor : IDisposable {
    public sealed class Record {
        public uint Id, Parent, ExitCode;
        public ulong Time;
        public string Name, Executable;
        public bool Started;
        public bool Instrumented;
    }
    sealed class Entry { public uint Id; public string Path; public IntPtr Handle; public bool Instrumented; }
    [StructLayout(LayoutKind.Sequential)] struct Port { public IntPtr Key, Handle; }
    [StructLayout(LayoutKind.Sequential)] struct Accounting { public long User, Kernel, PeriodUser, PeriodKernel; public uint Faults, Total, Active, Terminated; }
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct Startup {
        public uint Size; public string Reserved, Desktop, Title;
        public uint X,Y,Width,Height,CharsX,CharsY,Fill,Flags;
        public ushort Show, ReservedSize; public IntPtr ReservedData, Input, Output, Error;
    }
    [StructLayout(LayoutKind.Sequential)] struct ProcessInfo { public IntPtr Process, Thread; public uint Id, ThreadId; }
    [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr CreateJobObjectW(IntPtr attributes, IntPtr name);
    [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr CreateIoCompletionPort(IntPtr file, IntPtr existing, IntPtr key, uint threads);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetInformationJobObject(IntPtr job, int kind, ref Port info, uint length);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool QueryInformationJobObject(IntPtr job, int kind, out Accounting info, uint length, IntPtr returned);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool TerminateJobObject(IntPtr job, uint exitCode);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetQueuedCompletionStatus(IntPtr port, out uint bytes, out IntPtr key, out IntPtr data, uint wait);
    [DllImport("kernel32.dll")] static extern bool PostQueuedCompletionStatus(IntPtr port, uint bytes, IntPtr key, IntPtr data);
    [DllImport("kernel32.dll",SetLastError=true,CharSet=CharSet.Unicode)] static extern bool CreateProcessW(string application, StringBuilder command, IntPtr processAttributes, IntPtr threadAttributes, bool inherit, uint flags, IntPtr environment, string directory, ref Startup startup, out ProcessInfo info);
    [DllImport("kernel32.dll",SetLastError=true,CharSet=CharSet.Unicode)] static extern IntPtr CreateFileW(string path, uint access, uint share, IntPtr attributes, uint disposition, uint flags, IntPtr template);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetHandleInformation(IntPtr handle, uint mask, uint flags);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint ResumeThread(IntPtr thread);
    [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access, bool inherit, uint id);
    [DllImport("kernel32.dll",SetLastError=true,CharSet=CharSet.Unicode)] static extern bool QueryFullProcessImageNameW(IntPtr process, uint flags, StringBuilder name, ref uint capacity);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr process, out uint code);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    readonly IntPtr job, port;
    readonly Thread reader;
    readonly Dictionary<uint,Entry> entries=new Dictionary<uint,Entry>();
    readonly object gate=new object();
    readonly string compilerTelemetry;
    readonly string sanitizer;
    readonly List<string> infrastructureCleanup=new List<string>();
    public string[] InfrastructureCleanup { get { lock(gate) return infrastructureCleanup.ToArray(); } }
    public string ObservationError { get; private set; }
    static void Require(bool value) { if(!value) throw new Win32Exception(Marshal.GetLastWin32Error()); }
    static bool HasSanitizerRuntime(string path, string sanitizer) {
        using(var stream=System.IO.File.Open(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete))
        using(var reader=new BinaryReader(stream)) {
            stream.Position=60; uint nt=reader.ReadUInt32(); stream.Position=nt;
            if(reader.ReadUInt32()!=0x4550) return false;
            stream.Position=nt+6; ushort sections=reader.ReadUInt16(); stream.Position=nt+20; ushort optionalSize=reader.ReadUInt16();
            long optional=nt+24; stream.Position=optional; bool wide=reader.ReadUInt16()==0x20b;
            stream.Position=optional+(sanitizer=="Undefined" ? (wide?112:96) : (wide?120:104)); uint imports=reader.ReadUInt32();
            var ranges=new List<uint[]>();
            for(int i=0;i<sections;++i) {
                stream.Position=optional+optionalSize+i*40+8;
                uint size=reader.ReadUInt32(),address=reader.ReadUInt32(),rawSize=reader.ReadUInt32(),raw=reader.ReadUInt32();
                ranges.Add(new uint[]{address,Math.Max(size,rawSize),raw});
            }
            Func<uint,long> offset=rva=> { foreach(var range in ranges) if(rva>=range[0] && rva<range[0]+range[1]) return range[2]+rva-range[0]; return rva; };
            if(imports==0) return false;
            if(sanitizer=="Undefined") {
                long exports=offset(imports);
                stream.Position=exports+24; uint count=reader.ReadUInt32();
                stream.Position=exports+32; uint names=reader.ReadUInt32();
                if(count>65536) throw new InvalidDataException("Invalid PE export count.");
                for(uint i=0;i<count;++i) {
                    stream.Position=offset(names)+i*4; uint name=reader.ReadUInt32();
                    stream.Position=offset(name); var text=new StringBuilder();
                    for(int n=0;n<256;++n) { byte ch=reader.ReadByte(); if(ch==0) break; text.Append((char)ch); }
                    if(text.ToString().StartsWith("__ubsan_handle_",StringComparison.Ordinal)) return true;
                }
                return false;
            }
            long descriptor=offset(imports);
            for(int i=0;i<4096;++i,descriptor+=20) {
                stream.Position=descriptor+12; uint name=reader.ReadUInt32(); if(name==0) return false;
                stream.Position=offset(name); var text=new StringBuilder();
                for(int n=0;n<256;++n) { byte ch=reader.ReadByte(); if(ch==0) break; text.Append((char)ch); }
                if(text.ToString().StartsWith("clang_rt.asan",StringComparison.OrdinalIgnoreCase)) return true;
            }
            throw new InvalidDataException("Unterminated PE import table.");
        }
    }
    public SanitizerProcessMonitor(string compilerTelemetryPath=null, string sanitizerMode="Address") {
        if(sanitizerMode!="Address" && sanitizerMode!="Undefined") throw new ArgumentException("Unknown sanitizer mode.");
        sanitizer=sanitizerMode;
        compilerTelemetry=compilerTelemetryPath;
        job=CreateJobObjectW(IntPtr.Zero,IntPtr.Zero); Require(job!=IntPtr.Zero);
        port=CreateIoCompletionPort(new IntPtr(-1),IntPtr.Zero,IntPtr.Zero,1); Require(port!=IntPtr.Zero);
        var association=new Port {Key=job,Handle=port};
        Require(SetInformationJobObject(job,7,ref association,(uint)Marshal.SizeOf<Port>()));
        reader=new Thread(Read) {IsBackground=true}; reader.Start();
    }
    void Observe(uint id, IntPtr existing) {
        lock(gate) {
            if(entries.ContainsKey(id)) return;
            IntPtr handle=existing==IntPtr.Zero ? OpenProcess(0x1000|0x100000,false,id) : existing;
            var path=new StringBuilder(32768); uint size=(uint)path.Capacity;
            bool named=handle!=IntPtr.Zero && QueryFullProcessImageNameW(handle,0,path,ref size);
            bool instrumented=false;
            try { if(named) instrumented=HasSanitizerRuntime(path.ToString(),sanitizer); }
            catch(IOException) { ObservationError="A launched executable could not be inspected."; }
            catch(UnauthorizedAccessException) { ObservationError="A launched executable could not be inspected."; }
            entries.Add(id,new Entry {Id=id,Handle=handle,Path=named ? path.ToString() : null,Instrumented=instrumented});
        }
    }
    void Read() {
        for(;;) {
            uint message; IntPtr key,data;
            if(!GetQueuedCompletionStatus(port,out message,out key,out data,0xFFFFFFFF)) return;
            if(key==IntPtr.Zero) return;
            if(message==6) Observe(unchecked((uint)data.ToInt64()),IntPtr.Zero);
        }
    }
    static IntPtr File(string path, uint access, uint disposition) {
        IntPtr file=CreateFileW(path,access,3,IntPtr.Zero,disposition,0x80,IntPtr.Zero);
        Require(file!=new IntPtr(-1)); Require(SetHandleInformation(file,1,1)); return file;
    }
    public int Launch(string executable,string command,string directory,string stdout,string stderr) {
        IntPtr output=File(stdout,0x40000000,2), error=File(stderr,0x40000000,2), input=File("NUL",0x80000000,3);
        ProcessInfo info=new ProcessInfo();
        try {
            var startup=new Startup {Size=(uint)Marshal.SizeOf<Startup>(),Flags=0x100,Input=input,Output=output,Error=error};
            Require(CreateProcessW(executable,new StringBuilder(command),IntPtr.Zero,IntPtr.Zero,true,0x08000004,IntPtr.Zero,directory,ref startup,out info));
            Require(AssignProcessToJobObject(job,info.Process));
            Observe(info.Id,info.Process);
            // The notification may have opened its own handle first.
            lock(gate) { if(entries[info.Id].Handle!=info.Process) CloseHandle(info.Process); }
            Require(ResumeThread(info.Thread)!=0xFFFFFFFF);
            return (int)info.Id;
        } finally {
            if(info.Thread!=IntPtr.Zero) CloseHandle(info.Thread);
            CloseHandle(output); CloseHandle(error); CloseHandle(input);
        }
    }
    public bool WaitForCompletion(int milliseconds) {
        var deadline=DateTime.UtcNow.AddMilliseconds(milliseconds);
        do {
            Accounting accounting;
            Require(QueryInformationJobObject(job,1,out accounting,(uint)Marshal.SizeOf<Accounting>(),IntPtr.Zero));
            lock(gate) {
                if(accounting.Active==0 && accounting.Total==entries.Count) return true;
                // CL telemetry can outlive compilation. Clean only exact selected-
                // toolchain paths in this job, after all other children exited.
                if(compilerTelemetry!=null && accounting.Total==entries.Count && accounting.Active>0) {
                    var active=new List<Entry>(); bool complete=true, compilerSeen=false;
                    foreach(var entry in entries.Values) {
                        uint code;
                        if(entry.Handle==IntPtr.Zero || entry.Path==null || !GetExitCodeProcess(entry.Handle,out code)) { complete=false; break; }
                        if(code==259) active.Add(entry);
                        if(String.Equals(entry.Path,Path.Combine(Path.GetDirectoryName(compilerTelemetry),"cl.exe"),StringComparison.OrdinalIgnoreCase)) compilerSeen=true;
                    }
                    bool onlyTelemetry=complete && compilerSeen && active.Count==accounting.Active;
                    foreach(var entry in active) {
                        bool allowed=String.Equals(entry.Path,compilerTelemetry,StringComparison.OrdinalIgnoreCase);
                        if(entry.Instrumented || !allowed) onlyTelemetry=false;
                    }
                    if(onlyTelemetry) {
                        foreach(var entry in active) infrastructureCleanup.Add(entry.Path);
                        Require(TerminateJobObject(job,0));
                    }
                }
            }
            Thread.Sleep(50);
        } while(DateTime.UtcNow<deadline);
        return false;
    }
    public Record[] Snapshot() {
        Accounting accounting;
        Require(QueryInformationJobObject(job,1,out accounting,(uint)Marshal.SizeOf<Accounting>(),IntPtr.Zero));
        var result=new List<Record>();
        lock(gate) {
            if(accounting.Total!=entries.Count || accounting.Active!=0) ObservationError="Job accounting does not match a fully observed, completed process tree.";
            foreach(var entry in entries.Values) {
                uint code=259;
                if(entry.Handle==IntPtr.Zero || entry.Path==null || !GetExitCodeProcess(entry.Handle,out code) || code==259)
                    ObservationError="A child process identity/exit status could not be established.";
                result.Add(new Record {Id=entry.Id,Name=entry.Path==null ? "<unknown>" : Path.GetFileName(entry.Path),Executable=entry.Path,Started=false,ExitCode=code,Instrumented=entry.Instrumented});
            }
        }
        return result.ToArray();
    }
    public void Dispose() {
        TerminateJobObject(job,1);
        PostQueuedCompletionStatus(port,0,IntPtr.Zero,IntPtr.Zero); reader.Join();
        lock(gate) foreach(var entry in entries.Values) if(entry.Handle!=IntPtr.Zero) CloseHandle(entry.Handle);
        CloseHandle(job); CloseHandle(port);
    }
}
