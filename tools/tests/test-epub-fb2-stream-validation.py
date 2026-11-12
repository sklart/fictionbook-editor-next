import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT=Path(__file__).parent/'manual/import-epub/validate-fb2-stream.py'


class StreamValidation(unittest.TestCase):
    def test_schema_accepts_and_rejects_and_protects_inputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder=Path(temporary)
            schema=folder/'schema.xsd'
            schema.write_text('<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="root"><xs:complexType><xs:sequence><xs:element name="item" type="xs:string"/></xs:sequence></xs:complexType></xs:element></xs:schema>',encoding='utf8')
            source=folder/'input.xml'
            for index,(text,expected) in enumerate((('<root><item>text</item></root>','PASS'),('<root/>','FAIL'))):
                source.write_text(text,encoding='utf8')
                report=folder/f'{index}.json'
                result=subprocess.run([sys.executable,str(SCRIPT),'--input',str(source),'--schema',str(schema),'--report',str(report)],capture_output=True)
                self.assertEqual(result.returncode,0 if expected=='PASS' else 1)
                data=json.loads(report.read_text(encoding='utf8'))
                self.assertEqual(data['status'],expected)
                self.assertEqual(len(data['schema_sha256']),64)
            original=source.read_bytes()
            result=subprocess.run([sys.executable,str(SCRIPT),'--input',str(source),'--schema',str(schema),'--report',str(source)],capture_output=True)
            self.assertNotEqual(result.returncode,0)
            self.assertEqual(source.read_bytes(),original)


if __name__=='__main__':unittest.main()
