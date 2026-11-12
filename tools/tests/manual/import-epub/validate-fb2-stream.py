"""Validate a large FB2 incrementally against a local schema."""
import argparse
import hashlib
import json
from pathlib import Path
from lxml import etree


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--input',type=Path,required=True)
    p.add_argument('--schema',type=Path,required=True)
    p.add_argument('--report',type=Path,required=True)
    args=p.parse_args()
    report=args.report.resolve()
    if report.exists() or report in (args.input.resolve(),args.schema.resolve()):p.error('report must be a new file')
    schema=etree.XMLSchema(etree.parse(str(args.schema)))
    schema_sha256=hashlib.sha256(args.schema.read_bytes()).hexdigest()
    sha=hashlib.sha256()
    with args.input.open('rb') as stream:
        for chunk in iter(lambda:stream.read(1024*1024),b''):sha.update(chunk)
    result={'input_sha256':sha.hexdigest(),'schema_sha256':schema_sha256,'status':'ERROR'}
    try:
        for _,node in etree.iterparse(str(args.input),events=('end',),schema=schema,
                                      resolve_entities=False,no_network=True,huge_tree=True):
            node.clear()
            while node.getprevious() is not None:del node.getparent()[0]
        result['status']='PASS'
    except (etree.XMLSyntaxError,etree.DocumentInvalid) as error:
        result['status']='FAIL';result['error']=str(error)[:2000]
    report.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False))
    return 0 if result['status']=='PASS' else 1


if __name__=='__main__':raise SystemExit(main())
