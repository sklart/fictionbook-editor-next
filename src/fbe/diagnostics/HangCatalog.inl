// Included in DiagnosticPackage.cpp's private namespace.
    struct HangFile { CString path, session; };
    bool HangFileName(const CString& name,CString& id)
    {
        if(name.Left(9)!=L"fbe-hang-" || name.GetLength()<51 || name.Right(4)!=L".txt" || name[45]!=L'-') return false;
        id=name.Mid(9,36); const CString number=name.Mid(46,name.GetLength()-50);
        return ValidSessionId(id) && NumericText(number,L"") && number.GetLength()<=10 && _wtoi64(number)>0 && _wtoi64(number)<=MAXDWORD;
    }
    std::vector<HangFile> HangFiles(const CString& directory)
    {
        std::vector<HangFile> files; CString root(directory); root.TrimRight(L"\\/"); root+=L"\\Hangs\\";
        const DWORD attributes=::GetFileAttributes(root);
        if(attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY) || (attributes&FILE_ATTRIBUTE_REPARSE_POINT)) return files;
        WIN32_FIND_DATA data={}; HANDLE search=::FindFirstFile(root+L"fbe-hang-*.txt",&data);
        if(search==INVALID_HANDLE_VALUE) return files;
        do {
            if(TaskCancelled()) break;
            const CString name(data.cFileName);
            CString id;
            if((data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || !HangFileName(name,id)) continue;
            files.push_back({root+name,id});
        } while(::FindNextFile(search,&data)); ::FindClose(search);
        std::sort(files.begin(),files.end(),[](const HangFile& a,const HangFile& b){return a.path.Compare(b.path)<0;});
        return files;
    }
