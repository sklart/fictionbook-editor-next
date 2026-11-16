import argparse,importlib.util,unittest,os,subprocess,tempfile,zipfile,time
from pathlib import Path
spec=importlib.util.spec_from_file_location('fidelity',str(Path(__file__).resolve().parent/'test-import-epub-fidelity.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
parser=argparse.ArgumentParser()
parser.add_argument('--batch',type=Path,required=True)
args,rest=parser.parse_known_args()
m.BATCH=args.batch.resolve()
class LongArchive(m.Fidelity):
 def test_long_archive_and_cleanup(self):
  name='D'*90+'/'+'E'*90+'/'+'F'*90+'/chapter.xhtml'
  self.assertGreater(len(name),260)
  root=self.convert('<p>control</p>',extra={name:('long','application/xhtml+xml','<p>Long archive body.</p>')},spine=['long'],navigation=False,temp_padding=45)
  self.assertIn('Long archive body.',''.join(root.itertext()))
  scratch=self.last_folder/('t'*45)
  self.assertEqual([],list(scratch.iterdir()),'Temporary extraction was not cleaned up')
 def test_long_unicode_archive_and_cleanup(self):
  name='Книга с пробелом' * 6 + '/' + 'Главы' * 18 + '/' + 'текст' * 20 + '.xhtml'
  self.assertGreater(len(name),260)
  root=self.convert('<p>control</p>',extra={name:('long','application/xhtml+xml','<p>Unicode long body.</p>')},spine=['long'],navigation=False,temp_padding=45)
  self.assertIn('Unicode long body.',''.join(root.itertext()))
  self.assertEqual([],list((self.last_folder/('t'*45)).iterdir()))
 def test_encoded_escape_with_long_temporary_path(self):
  root=self.convert('<p>Safe<img src="%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2foutside.png"/></p>',outside={'outside.png':m.PNG},temp_padding=45)
  self.assertEqual(root.findall('f:binary',m.NS),[])
  self.assertIn('Unresolved image resource',self.diagnostics)
  self.assertEqual((self.last_folder/('t'*45)/'outside.png').read_bytes(),m.PNG)
  self.assertEqual(['outside.png'],[p.name for p in (self.last_folder/('t'*45)).iterdir()])
 def test_long_resource_relative_parent_and_image_bytes(self):
  directory='К'*90+'/'+'B'*90+'/'+'C'*90
  chapter=directory+'/inner/chapter.xhtml'
  image=directory+'/media/рисунок 1.png'
  root=self.convert('<p>control</p>',extra={chapter:('long','application/xhtml+xml','<p>Before<img src="../media/%D1%80%D0%B8%D1%81%D1%83%D0%BD%D0%BE%D0%BA%201.png"/>After</p>'),image:('picture','image/png',m.PNG)},spine=['long'],navigation=False,temp_padding=45)
  self.assertIn('BeforeAfter',''.join(root.itertext()).replace('\n','').replace(' ',''))
  binaries=root.findall('f:binary',m.NS)
  self.assertEqual(len(binaries),1)
  self.assertEqual(m.base64.b64decode(binaries[0].text or ''),m.PNG)
  self.assertEqual([],list((self.last_folder/('t'*45)).iterdir()))
 def test_valid_xml_without_recovery_warning(self):
  root=self.convert('<p>Ordinary valid content.</p>',navigation=False)
  self.assertIn('Ordinary valid content.',''.join(root.itertext()))
  self.assertNotIn('Recovered malformed XHTML',self.diagnostics)
 def test_long_xml_stream_preserves_malformed_html_fallback(self):
  source=b'<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Chapter</title></head><body><p>First<br>Second</p></body></html>'
  root=self.convert('',chapter_bytes=source,navigation=False,temp_padding=220)
  self.assertIn('First',''.join(root.itertext()))
  self.assertIn('Second',''.join(root.itertext()))
  self.assertIn('Recovered malformed XHTML',self.diagnostics)
 def test_long_xml_stream_does_not_load_external_entity(self):
  temp=tempfile.TemporaryDirectory(prefix='epub-long-entity-');self.folders.append(temp)
  root=Path(temp.name);scratch=root/('t'*45);scratch.mkdir()
  secret=root/'outside.txt';secret.write_text('PRIVATE_ENTITY_SENTINEL',encoding='utf-8')
  epub=root/'input.epub';output=root/'output.fb2';longpath='D'*90+'/'+'E'*90+'/'+'F'*90+'/chapter.xhtml'
  with zipfile.ZipFile(epub,'w') as z:
   z.writestr('mimetype','application/epub+zip')
   z.writestr('META-INF/container.xml','<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
   z.writestr('OEBPS/book.opf',f'<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Fixture</dc:title><dc:language>en</dc:language><dc:identifier id="uid">test</dc:identifier></metadata><manifest><item id="c" href="{longpath}" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="c"/></spine></package>')
   z.writestr('OEBPS/'+longpath,f'<!DOCTYPE html [<!ENTITY xxe SYSTEM "{secret.as_uri()}">]><html xmlns="http://www.w3.org/1999/xhtml"><head><title>Chapter</title></head><body><p>Safe &xxe;</p></body></html>')
  env=os.environ.copy();env['TEMP']=env['TMP']=str(scratch)
  result=subprocess.run([str(m.BATCH),str(epub),str(output)],capture_output=True,timeout=30,env=env)
  self.assertEqual(result.returncode,0,result.stdout+result.stderr)
  text=''.join(m.ET.parse(output).getroot().itertext())
  self.assertIn('Safe',text)
  self.assertNotIn('PRIVATE_ENTITY_SENTINEL',text)
  self.assertEqual(secret.read_text(encoding='utf-8'),'PRIVATE_ENTITY_SENTINEL')
  self.assertEqual([],list(scratch.iterdir()))
  self.outputs.append(output)
 def test_traversal_and_failure_cleanup_with_long_temporary_path(self):
  with tempfile.TemporaryDirectory(prefix='epub-long-security-') as folder:
   root=Path(folder);scratch=root/('t'*45);scratch.mkdir();sentinel=root/'outside.txt';sentinel.write_bytes(b'unchanged')
   epub=root/'bad.epub';output=root/'bad.fb2'
   with zipfile.ZipFile(epub,'w') as z:
    z.writestr('mimetype','application/epub+zip')
    z.writestr('OEBPS/'+'a'*90+'/'+'b'*90+'/'+'c'*90+'/ok.txt','ok')
    z.writestr('OEBPS/'+'a'*90+'/'+'b'*90+'/'+'c'*90+'/../../../../outside.txt','attack')
   env=os.environ.copy();env['TEMP']=env['TMP']=str(scratch)
   result=subprocess.run([str(m.BATCH),str(epub),str(output)],capture_output=True,timeout=30,env=env)
   self.assertNotEqual(result.returncode,0)
   self.assertFalse(output.exists())
   self.assertEqual(sentinel.read_bytes(),b'unchanged')
   self.assertEqual([],list(scratch.iterdir()),'Failed extraction was not cleaned up')
 def test_temporary_root_over_max_path(self):
  temp=tempfile.TemporaryDirectory(prefix='epub-long-temp-');self.folders.append(temp)
  root=Path(temp.name);scratch=root/('a'*90)/('b'*90)/('c'*90)
  self.assertGreater(len(str(scratch)),260)
  long_scratch='\\\\?\\'+str(scratch)
  os.makedirs(long_scratch)
  epub=root/'input.epub';output=root/'output.fb2'
  with zipfile.ZipFile(epub,'w') as z:
   z.writestr('mimetype','application/epub+zip')
   z.writestr('META-INF/container.xml','<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
   z.writestr('OEBPS/book.opf','<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Fixture</dc:title><dc:language>en</dc:language><dc:identifier id="uid">test</dc:identifier></metadata><manifest><item id="c" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="c"/></spine></package>')
   z.writestr('OEBPS/chapter.xhtml','<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Chapter</title></head><body><p>Long temporary root.</p></body></html>')
  env=os.environ.copy();env['TEMP']=env['TMP']=str(scratch)
  env['FBE_NEXT_TEST_MODE']='1';env['FBE_NEXT_TEST_EPUB_DELAY_ENTRY']='chapter.xhtml'
  env['FBE_NEXT_TEST_EPUB_ROOT_REPORT']=str(root/'actual-extraction-root.txt')
  result=subprocess.run([str(m.BATCH),str(epub),str(output)],capture_output=True,timeout=30,env=env)
  self.assertEqual(result.returncode,0,result.stdout+result.stderr)
  actual=(root/'actual-extraction-root.txt').read_text(encoding='utf-8')
  self.assertTrue(actual.casefold().startswith((long_scratch+'\\').casefold()),'Extraction did not use the requested TEMP directory')
  self.assertIn('Long temporary root.',''.join(m.ET.parse(output).getroot().itertext()))
  self.outputs.append(output)
  self.assertEqual([],os.listdir(long_scratch),'Temporary extraction was not cleaned up')
 def test_reparse_race_in_long_extraction_path(self):
  with tempfile.TemporaryDirectory(prefix='epub-long-race-') as folder:
   root=Path(folder);scratch=root/('t'*45);scratch.mkdir();outside=root/'outside';outside.mkdir()
   sentinel=outside/'sentinel.txt';sentinel.write_bytes(b'unchanged')
   epub=root/'race.epub';output=root/'race.fb2';report=root/'root.txt'
   with zipfile.ZipFile(epub,'w') as z:
    z.writestr('mimetype','application/epub+zip')
    z.writestr('OEBPS/'+'x'*100+'/'+'y'*100+'/item.xhtml','<p>unexpected</p>')
   env=os.environ.copy();env['TEMP']=env['TMP']=str(scratch)
   env['FBE_NEXT_TEST_MODE']='1';env['FBE_NEXT_TEST_EPUB_DELAY_ENTRY']='item.xhtml';env['FBE_NEXT_TEST_EPUB_ROOT_REPORT']=str(report)
   proc=subprocess.Popen([str(m.BATCH),str(epub),str(output)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env)
   link=None
   try:
    deadline=time.monotonic()+10
    while not report.exists() and proc.poll() is None and time.monotonic()<deadline: time.sleep(0.02)
    self.assertTrue(report.exists(),'Extraction did not reach the controlled boundary')
    extraction=report.read_text(encoding='utf-8');link=extraction+'\\OEBPS'
    result=subprocess.run(['cmd.exe','/d','/c','mklink','/J',link,str(outside)],capture_output=True,timeout=10)
    self.assertEqual(result.returncode,0,result.stdout+result.stderr)
    out,err=proc.communicate(timeout=30)
    self.assertNotEqual(proc.returncode,0,out+err)
    self.assertFalse(output.exists())
    self.assertEqual(sentinel.read_bytes(),b'unchanged')
   finally:
    if proc.poll() is None: proc.kill();proc.communicate()
    if link and os.path.lexists(link):
     attr=os.lstat(link).st_file_attributes
     if not attr & 0x400: raise RuntimeError('Refusing to remove a non-reparse-point')
     os.rmdir(link)
   self.assertEqual(sentinel.read_bytes(),b'unchanged')
   self.assertEqual([],list(scratch.iterdir()))
if __name__=='__main__':
 unittest.main(argv=[__file__,*rest],defaultTest=['LongArchive.test_long_archive_and_cleanup', 'LongArchive.test_long_unicode_archive_and_cleanup', 'LongArchive.test_encoded_escape_with_long_temporary_path', 'LongArchive.test_long_resource_relative_parent_and_image_bytes', 'LongArchive.test_valid_xml_without_recovery_warning', 'LongArchive.test_long_xml_stream_preserves_malformed_html_fallback', 'LongArchive.test_long_xml_stream_does_not_load_external_entity', 'LongArchive.test_traversal_and_failure_cleanup_with_long_temporary_path', 'LongArchive.test_temporary_root_over_max_path', 'LongArchive.test_reparse_race_in_long_extraction_path'])
