[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),[int]$TimeoutSeconds=180)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name binary-scripting-compatibility
$passed=$false
$evidence=Join-Path $root 'out/tests/binary-store/compatibility'
New-Item -ItemType Directory -Force $evidence | Out-Null
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TRACE')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
    $null=New-IsolatedFbeSettingsSeed $iso
    Add-Type -AssemblyName System.Drawing
    $payloads=@{}
    foreach($spec in @(@('A',3,2,'png','Red',4096),@('B',3,2,'png','Blue',4096),@('C',7,5,'jpeg','Green',0),@('D',11,9,'png','Gold',0),@('large',4096,2048,'png','Purple',0))) {
        $bitmap=[Drawing.Bitmap]::new([int]$spec[1],[int]$spec[2]);$graphics=[Drawing.Graphics]::FromImage($bitmap);$stream=[IO.MemoryStream]::new()
        try {
            $graphics.Clear([Drawing.Color]::FromName($spec[4]));$bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::($spec[3]));$bytes=$stream.ToArray()
            if($spec[5]){$padded=[byte[]]::new([int]$spec[5]);$bytes.CopyTo($padded,0);$bytes=$padded}
            $payloads[$spec[0]]=@{bytes=$bytes;mime=('image/'+$spec[3]);width=[int]$spec[1];height=[int]$spec[2]}
        } finally {$stream.Dispose();$graphics.Dispose();$bitmap.Dispose()}
    }
    $payloads['blob']=@{bytes=[byte[]](0..16);mime='application/octet-stream';width=$null;height=$null}
    $payloads['small']=@{bytes=[byte[]]@(255);mime='application/octet-stream';width=$null;height=$null}
    $payloads['empty']=@{bytes=[byte[]]@();mime='application/octet-stream';width=$null;height=$null}
    $payloads['truncated']=@{bytes=[byte[]]@(137,80,78,71,13,10,26,10);mime='image/png';width=$null;height=$null}
    $data=@{}
    foreach($key in $payloads.Keys){
        $p=$payloads[$key];$file="fixture-$key.dat";[IO.File]::WriteAllBytes((Join-Path $iso.Root $file),$p.bytes)
        $data[$key]=@{base64=[Convert]::ToBase64String($p.bytes);size=$p.bytes.Length;mime=$p.mime;width=$p.width;height=$p.height;file=$file;sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($p.bytes)).ToLowerInvariant()}
    }
    if($data.A.size -ne $data.B.size -or $data.A.sha256 -eq $data.B.sha256){throw 'Same-size/dimensions fixture invalid'}
    $description='<description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Binary scripting compatibility</book-title><coverpage><image l:href="#image1.png"/></coverpage><lang>en</lang></title-info><document-info><id>binary-compat</id><version>1</version></document-info></description>'
    function Write-Book([string]$Name,[string]$Main) {
        $items=[ordered]@{'image1.png'=$Main;'photo.jpg'='C';'blob.bin'='blob';'small.bin'='small';'duplicate.png'='A';'unused.bin'='blob'}
        $binaries='';foreach($id in $items.Keys){$p=$data[$items[$id]];$binaries+='<binary id="'+$id+'" content-type="'+$p.mime+'">'+$p.base64+'</binary>'}
        $book='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">'+$description+'<body><section><p>Shared <image l:href="#image1.png"/> references <image l:href="#image1.png"/>.</p></section></body>'+$binaries+'</FictionBook>'
        [IO.File]::WriteAllText((Join-Path $iso.Root $Name),$book,[Text.UTF8Encoding]::new($false))
    }
    Write-Book 'book-a.fb2' A;Write-Book 'book-b.fb2' D
    $helpers=@'
var F=@@FIXTURES@@,e=window.external,compatResult="NOT RUN";
function A(ok,label){if(!ok)throw new Error(label);}
function Obj(id){var all=document.all.binobj.getElementsByTagName("DIV");for(var i=0;i<all.length;i++)if(all[i].all.id.value===id)return all[i];return null;}
function Data(k){var n=new ActiveXObject("Msxml2.DOMDocument.6.0");n.loadXML("<data/>");n.documentElement.dataType="bin.base64";n.documentElement.text=F[k].base64;return n.documentElement.nodeTypedValue;}
function Enc(value){if(value==null)return "";var n=new ActiveXObject("Msxml2.DOMDocument.6.0");n.loadXML("<data/>");n.documentElement.dataType="bin.base64";n.documentElement.nodeTypedValue=value;return String(n.documentElement.text).replace(/\s/g,"");}
function Info(id){return eval("("+e.GetBinaryInfo(id)+")");}
function Check(id,k){var b=Obj(id),p=F[k];A(!!b,"missing "+id);A(Enc(b.base64data)===p.base64,"legacy bytes "+id);A(Enc(b.base64data)===p.base64,"repeat legacy read "+id);try{A(e.GetBinarySize(b.base64data)===p.size,"legacy size "+id);}catch(error){if(p.size!==0)throw error;}var info=null;try{info=Info(id);}catch(error){if(p.size!==0)throw error;}if(info){A(info.id===id&&info.size===p.size&&info.mimeType===b.all.type.value,"metadata "+id);A(info.width===p.width&&info.height===p.height,"dimensions "+id);}if(p.width!=null)A(e.GetImageDimsByData(b.base64data)===p.width+"x"+p.height,"legacy decoder "+id);}
function Raw(k,id){var b=Obj(id||"image1.png");b.base64data=Data(k);if(b.all.type.value!==F[k].mime)b.all.type.value=F[k].mime;}
function Absent(id){A(!Obj(id),"DOM ID present "+id);var rejected=false;try{Info(id);}catch(error){rejected=true;}A(rejected,"stale metadata "+id);}
function Refs(id){var images=document.getElementById("fbw_body").getElementsByTagName("IMG"),count=0;for(var i=0;i<images.length;i++){var href=String(images[i].parentNode.getAttribute("href"));if(href==="#"+id){A(Enc(GetImageData(id))===Enc(Obj(id).base64data),"shared reference bytes");count++;}}A(count>=2,"shared reference count");}
function CompatResult(){return compatResult;}
'@
    $helpers=$helpers.Replace('@@FIXTURES@@',($data | ConvertTo-Json -Depth 5 -Compress))
    $manifest=[xml]'<suite/>';$steps=[Collections.Generic.List[object]]::new()
    # Keep generated scripts byte-for-byte independent of product implementation.
    function Add-Case([hashtable]$Attrs,[string]$Body='') {
        if($Body){
            $file=('case-'+$steps.Count+'.js');$Attrs.script=$file
            $text=$helpers+"`r`nfunction Run(){try{`r`n"+$Body+"`r`ncompatResult='PASS';}catch(error){compatResult='FAIL: '+String(error.message||error);}}`r`n"
            [IO.File]::WriteAllText((Join-Path $iso.Root $file),$text,[Text.UTF8Encoding]::new($false))
        }
        $node=$manifest.CreateElement('step');foreach($key in $Attrs.Keys){$node.SetAttribute($key,[string]$Attrs[$key])};[void]$manifest.DocumentElement.AppendChild($node)
        $record=@{save='';id='';expected='';absent=''};foreach($key in $Attrs.Keys){$record[$key]=$Attrs[$key]};$steps.Add([pscustomobject]$record)
    }
    function State([string]$Name,[string]$Key,[string]$Body,[string]$Save='') {
        $attrs=@{name=$Name;id='image1.png';expected=$data[$Key].file};if($Save){$attrs.save=$Save};Add-Case $attrs $Body
    }
    Add-Case @{name='initial-read-PNG-JPEG-blob-small';id='image1.png';expected=$data.A.file;readOnly100='true'} 'A(e.GetScriptingApiVersion()==="1.5.0","public API version");A(!e.HasScriptingMethod("BinaryStore")&&!e.HasScriptingMethod("_BinaryStoreChanged")&&!e.HasScriptingMethod("_BinaryStoreUniqueId"),"store exposed as public API");Check("image1.png","A");Check("photo.jpg","C");Check("blob.bin","blob");Check("small.bin","small");Check("duplicate.png","A");A(Obj("image1.png")!==Obj("duplicate.png"),"semantic deduplication");Refs("image1.png");'
    Add-Case @{name='raw-B-same-size-dimensions';id='image1.png';expected=$data.B.file;save='raw-b.fb2';rawDirty='unchanged'} 'Raw("B");Check("image1.png","B");Refs("image1.png");'
    $reader='reader-c.js';[IO.File]::WriteAllText((Join-Path $iso.Root $reader),$helpers+"`r`nfunction Run(){try{Check('image1.png','C');compatResult='PASS';}catch(error){compatResult='FAIL';}}",[Text.UTF8Encoding]::new($false))
    Add-Case @{name='script-A-writes-B-reads-without-save';id='image1.png';expected=$data.C.file;reader=$reader;save='raw-c.fb2'} 'Raw("C");'
    State 'raw-D-latest-generation' D 'Raw("D");Check("image1.png","D");Refs("image1.png");' 'raw-d.fb2'
    State 'raw-A-again' A 'Raw("A");Check("image1.png","A");' 'raw-a.fb2'
    Add-Case @{name='raw-rename-preserves-old-reference-semantics';id='raw-renamed.png';expected=$data.A.file} 'Obj("image1.png").all.id.value="raw-renamed.png";Absent("image1.png");Check("raw-renamed.png","A");var imgs=document.getElementById("fbw_body").getElementsByTagName("IMG");A(String(imgs[0].parentNode.getAttribute("href"))==="#image1.png","raw rename invented reference updates");'
    Add-Case @{name='old-raw-ID-absent';id='image1.png';absent='true'}
    Add-Case @{name='reload-A';command='open';open='book-a.fb2';id='image1.png';expected=$data.A.file}
    foreach($key in @('B','C')) {State ("script-owned-replace-$key") $key ('e.TraceScript("BC101","before directory");var compatDirectory=e.GetCurrentScriptDirectory();e.TraceScript("BC102","before path");var compatPath=e.CombinePath(compatDirectory,F["'+$key+'"].file);e.TraceScript("BC103","before replace");e.ReplaceBinaryFromFile("image1.png",compatPath,F["'+$key+'"].mime);e.TraceScript("BC104","after replace");Check("image1.png","'+$key+'");') ("owned-$key.fb2")}
    foreach($item in @(@('undo','B'),@('undo','A'),@('redo','B'),@('redo','C'))) {Add-Case @{name=($item[0]+'-'+$item[1]);command=$item[0];id='image1.png';expected=$data[$item[1]].file;save=($item[0]+'-'+$item[1]+'.fb2')}}
    State 'JPEG-to-PNG' D 'Raw("D");Check("image1.png","D");' 'format-png.fb2'
    Add-Case @{name='visual-source-visual';command='visual-source';id='image1.png';expected=$data.D.file}
    Add-Case @{name='source-mutation';command='source-mutate';base64=$data.A.base64;id='image1.png';expected=$data.A.file;save='source-a.fb2'}
    Add-Case @{name='legacy-delete';id='unused.bin';absent='true';save='deleted.fb2'} 'Obj("unused.bin").removeNode(true);FillCoverList();Absent("unused.bin");A(e.GenerateUniqueBinaryId("unused.bin")==="unused.bin","stale unique ID");'
    Add-Case @{name='legacy-apiAddBinary';id='added.png';expected=$data.D.file;save='added.fb2'} 'A(apiAddBinary("","added.png",F.D.mime,Data("D"))==="added.png","legacy add ID");Check("added.png","D");'
    Add-Case @{name='save-close-open';command='open';open='added.fb2';id='added.png';expected=$data.D.file}
    State 'duplicate-ID-first-DOM-wins' A 'var b=Obj("duplicate.png");b.base64data=Data("B");b.all.id.value="image1.png";Check("image1.png","A");Check("added.png","D");A(e.GenerateUniqueBinaryId("image1.png")!=="image1.png","duplicate occupancy");'
    State 'duplicate-ID-remove-first-next-visible' B 'Obj("image1.png").removeNode(true);FillCoverList();Check("image1.png","B");Refs("image1.png");' 'duplicate-next.fb2'
    Add-Case @{name='invalid-base64-value-does-not-corrupt-other-binary'} 'Obj("image1.png").base64data="not base64";var rejected=false;try{Info("image1.png");}catch(error){rejected=true;}A(rejected,"invalid legacy value accepted");Check("blob.bin","blob");'
    State 'restore-after-invalid-value' A 'Raw("A");Check("image1.png","A");'
    State 'truncated-image' truncated 'Raw("truncated");Check("image1.png","truncated");'
    State 'unknown-MIME' blob 'Raw("blob");Obj("image1.png").all.type.value="application/x-unknown";Check("image1.png","blob");'
    State 'zero-length-data' empty 'Raw("empty");Check("image1.png","empty");'
    State 'large-image' large 'Raw("large");Check("image1.png","large");Refs("image1.png");' 'large.fb2'
    Add-Case @{name='missing-reference';id='missing.png';absent='true'} 'var p=document.getElementById("fbw_body").getElementsByTagName("P")[0];var d=document.createElement("SPAN");d.className="image";d.setAttribute("href","#missing.png");var img=document.createElement("IMG");img.src="fbw-internal:#missing.png";d.appendChild(img);p.appendChild(d);Absent("missing.png");d.removeNode(true);'
    # The duplicate-ID fault deliberately bypassed oldId; reload a valid saved document before the normal UI rename path.
    Add-Case @{name='reload-after-duplicate-ID-fault';command='open';open='large.fb2';id='image1.png';expected=$data.large.file}
    Add-Case @{name='legacy-reference-aware-rename';id='renamed.png';expected=$data.large.file;save='renamed.fb2'} 'var b=Obj("image1.png");b.all.id.value="renamed.png";OnBinaryChange(b.all.id);Absent("image1.png");Check("renamed.png","large");Refs("renamed.png");'
    Add-Case @{name='old-ID-absent';id='image1.png';absent='true'}
    Add-Case @{name='reload-renamed';command='open';open='renamed.fb2';id='renamed.png';expected=$data.large.file}
    Add-Case @{name='close-A';command='close'}
    Add-Case @{name='new-after-close';command='new';id='image1.png';absent='true'}
    Add-Case @{name='open-B-same-ID-different-bytes';command='open';open='book-b.fb2';id='image1.png';expected=$data.D.file}
    Add-Case @{name='close-B';command='close'}
    Add-Case @{name='new-again';command='new';id='image1.png';absent='true'}
    Add-Case @{name='open-A-again';command='open';open='book-a.fb2';id='image1.png';expected=$data.A.file}
    $corpus=@(
        @('10_Статистика/03_Статистика документа.js','binary size/statistics'),
        @('05_Иллюстрации/04_Сохранить вложения на накопитель.js','legacy SaveBinary bytes'),
        @('05_Иллюстрации/03_Обновить иллюстрации.js','image refresh/shared references'),
        @('05_Иллюстрации/20_Обновить иллюстрации (версия Александра Ка).js','legacy dimensions/image metadata'),
        @('05_Иллюстрации/01_Проверить иллюстрации.js','image/binary references'),
        @('05_Иллюстрации/07_Удалить неиспользуемые вложения....js','legacy DOM remove unused binaries')
    )
    $corpusResults=@()
    foreach($item in $corpus){
        $source=Join-Path $root ('runtime/Scripts/'+$item[0]);$name='corpus-'+$corpusResults.Count+'.js';Copy-Item -LiteralPath $source -Destination (Join-Path $iso.Root $name)
        $corpusResults+=@{script=$item[0];operation=$item[1];sourceSha256=(Get-FileHash $source).Hash;stagedSha256=(Get-FileHash (Join-Path $iso.Root $name)).Hash;result='PENDING'}
        Add-Case @{name=('bundled-'+($corpusResults.Count));script=$name;corpus='true';id='image1.png';expected=$data.A.file;save=('corpus-'+$corpusResults.Count+'.fb2')}
    }
    Add-Case @{name='bundled-delete-removed-other-ID';id='photo.jpg';absent='true'}
    $manifest.Save((Join-Path $iso.Root 'suite.xml'))
    $data | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $evidence 'fixture-oracles.json') -Encoding utf8
    $steps | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'steps.json') -Encoding utf8
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='binary-scripting-compatibility';$env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings';$env:FBE_NEXT_TRACE='1'
    $report=Join-Path $iso.Root 'report.txt'
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,(Join-Path $iso.Root 'book-a.fb2')) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario binary-scripting-compatibility -Report $report -TimeoutSeconds $TimeoutSeconds
    if(Test-Path $report){Copy-Item $report (Join-Path $evidence 'report.txt') -Force}
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "Legacy scripting compatibility failed: $code; $text"}
    Copy-Item (Join-Path $iso.Root 'states.tsv') (Join-Path $evidence 'states.tsv') -Force
    $trace=Get-ChildItem (Join-Path $iso.DataPath 'Diagnostics') -Filter 'fbe-trace-*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $trace){throw 'Missing legacy compatibility diagnostic trace'}
    Copy-Item $trace.FullName (Join-Path $evidence 'trace.log') -Force
    if((Get-Content $trace.FullName -Raw) -match 'code=J900'){throw 'Unexpected script error (including bundled corpus); see trace.log'}
    $states=@(Import-Csv (Join-Path $iso.Root 'states.tsv') -Delimiter "`t")
    foreach($state in $states){
        $step=$steps | Where-Object name -eq $state.case | Select-Object -First 1
        $oracle=$data.Values | Where-Object file -eq $step.expected | Select-Object -First 1
        if($state.sha256 -cne $oracle.sha256 -or [long]$state.size -ne $oracle.size){throw "Hash/size: $($state.case)"}
        if($state.metadata -ne 'null'){$meta=$state.metadata | ConvertFrom-Json;if($meta.size -ne $oracle.size -or $meta.width -ne $oracle.width -or $meta.height -ne $oracle.height){throw "Metadata: $($state.case)"}}
    }
    foreach($step in $steps | Where-Object { $_.save }){
        $path=Join-Path $iso.Root $step.save
        $schema=New-Object -ComObject Msxml2.XMLSchemaCache.6.0;$schema.add('http://www.gribuser.ru/xml/fictionbook/2.0',(Join-Path $root 'runtime/FictionBook.xsd'))
        $doc=New-Object -ComObject Msxml2.DOMDocument.6.0;$doc.async=$false
        if(-not $doc.load($path)){throw 'Serialized XML failed to load'};$doc.schemas=$schema
        if($doc.validate().errorCode){throw "Serialized XSD failed: $($step.name)"}
        [xml]$xml=Get-Content $path -Raw;$ns=[Xml.XmlNamespaceManager]::new($xml.NameTable);$ns.AddNamespace('fb','http://www.gribuser.ru/xml/fictionbook/2.0')
        if($step.id -and -not $step.absent){$binary=$xml.SelectSingleNode('/fb:FictionBook/fb:binary[@id="'+$step.id+'"]',$ns);$expected=$data.Values | Where-Object file -eq $step.expected | Select-Object -First 1;if(-not $binary -or [Convert]::ToBase64String([Convert]::FromBase64String($binary.InnerText)) -cne $expected.base64){throw "Serialized bytes: $($step.name)"}}
        Copy-Item -LiteralPath $path -Destination $evidence -Force
    }
    foreach($entry in $corpusResults){$entry.result='PASS'}
    foreach($id in @('image1.png','photo.jpg','blob.bin','small.bin','duplicate.png','unused.bin')){
        $file=Join-Path $iso.Root $id
        if(-not (Test-Path $file)){throw "Bundled SaveBinary did not create $id"}
        $key=if($id -in @('image1.png','duplicate.png')){'A'}elseif($id -eq 'photo.jpg'){'C'}elseif($id -eq 'small.bin'){'small'}else{'blob'}
        if((Get-FileHash $file).Hash.ToLowerInvariant() -cne $data[$key].sha256){throw "Bundled SaveBinary bytes: $id"}
    }
    $corpusResults | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'corpus-results.json') -Encoding utf8
    [pscustomobject]@{status='PASS';passed=$steps.Count;total=$steps.Count;corpusPassed=$corpusResults.Count;corpusTotal=$corpusResults.Count;hashCache='NONE: hashes independently verified from actual bytes';binarySha256=(Get-FileHash $FbeExe).Hash;mainJsSha256=(Get-FileHash (Join-Path (Split-Path $FbeExe) 'main.js')).Hash;states=$states.Count} | ConvertTo-Json | Set-Content (Join-Path $evidence 'summary.json') -Encoding utf8
    $passed=$true;Write-Host "Legacy binary scripting compatibility: PASS $($steps.Count)/$($steps.Count); bundled corpus $($corpusResults.Count)/$($corpusResults.Count)."
} finally {
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
