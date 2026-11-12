"""Merge a completed corpus inventory and individual run records by SHA-256."""
import argparse
from collections import Counter,defaultdict
import csv
import hashlib
import json
from pathlib import Path
import sqlite3


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--inventory',type=Path,required=True)
    p.add_argument('--containers',type=Path,required=True)
    p.add_argument('--run',type=Path,action='append',required=True)
    p.add_argument('--retry',type=Path,action='append',default=[])
    p.add_argument('--xsd-recheck',type=Path,action='append',default=[])
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--csv',type=Path,required=True)
    args=p.parse_args()
    output,csv_path=args.output.resolve(),args.csv.resolve()
    if output.exists() or csv_path.exists() or output==csv_path:p.error('outputs must be new and distinct')
    source=sqlite3.connect(f'{args.inventory.resolve().as_uri()}?mode=ro',uri=True)
    source.row_factory=sqlite3.Row
    containers=sqlite3.connect(f'{args.containers.resolve().as_uri()}?mode=ro',uri=True)
    container_meta=dict(containers.execute('SELECT key,value FROM metadata'))
    root=Path(container_meta['root']).resolve(strict=True)
    if any(path==root or root in path.parents for path in (output,csv_path)):
        p.error('outputs must be outside the source tree')
    invalid={sha:status for sha,status in containers.execute('SELECT sha256,status FROM containers')}
    conversions={};identities=set()
    for run in args.run:
        run=run.resolve(strict=True)
        manifest=json.loads((run/'manifest.json').read_text(encoding='utf8'))
        identities.add((manifest['plugin_sha256'],manifest['batch_sha256'],manifest['comparator_sha256'],manifest['schema_sha256'],
                        manifest.get('svg_sha256'),manifest.get('fbe_sha256'),manifest.get('importer_source_sha256')))
        for file in run.rglob('result.json'):
            record=json.loads(file.read_text(encoding='utf8'))
            sha=record['source_sha256']
            if sha in conversions:raise ValueError(f'duplicate conversion results: {sha}')
            conversions[sha]=(record,str(file))
    if len(identities)!=1:raise ValueError('run identities differ')
    plugin_hash,_,comparator_hash,schema_hash,_,_,_=next(iter(identities))
    retries={}
    for path in args.retry:
        path=path.resolve(strict=True)
        retry=json.loads(path.read_text(encoding='utf-8-sig'))
        sha=retry['source_sha256']
        if sha in retries or sha not in conversions:raise ValueError('duplicate or unknown retry: '+sha)
        if (retry['plugin_sha256'],retry['schema_sha256'],retry['comparator_sha256']) != (plugin_hash,schema_hash,comparator_hash):
            raise ValueError('retry identity differs from corpus')
        for name,key in [('input.epub','source_sha256'),('output.fb2','output_sha256'),('comparison.json','comparison_sha256')]:
            with (path.parent/name).open('rb') as stream:
                actual=hashlib.file_digest(stream,'sha256').hexdigest()
            if actual!=retry[key]:raise ValueError('retry evidence hash mismatch: '+name)
        retries[sha]=(retry,str(path))
    checks={}
    output_to_source=defaultdict(list)
    for sha,(record,_) in conversions.items():
        if record.get('fb2_sha256'):output_to_source[record['fb2_sha256']].append(sha)
    for path in args.xsd_recheck:
        path=path.resolve(strict=True)
        check=json.loads(path.read_text(encoding='utf-8-sig'))
        sources=output_to_source.get(check['input_sha256'],[])
        if not sources or any(sha in checks for sha in sources) or check['schema_sha256']!=schema_hash:
            raise ValueError('unknown, duplicate or incompatible XSD recheck')
        for sha in sources:checks[sha]=(check,str(path))
    output.parent.mkdir(parents=True,exist_ok=True)
    db=sqlite3.connect(output)
    db.execute('CREATE TABLE books (id TEXT PRIMARY KEY, relative_path TEXT UNIQUE, size INTEGER, sha256 TEXT, metadata_json TEXT, history_json TEXT, source_error TEXT, status TEXT, xsd TEXT, comparison TEXT, result_path TEXT, retry_json TEXT, retry_path TEXT, xsd_recheck TEXT, xsd_recheck_path TEXT)')
    db.execute('CREATE INDEX books_sha256 ON books(sha256)')
    path_counts=Counter();unique_status={};rows=[]
    for row in source.execute('SELECT * FROM books ORDER BY relative_path'):
        sha=row['sha256'];record,path=conversions.get(sha,({},None))
        status=record.get('status','UNSUPPORTED' if invalid.get(sha)=='UNSUPPORTED_7Z' else 'NOT RUN')
        xsd=record.get('xsd','NOT RUN');comparison=record.get('comparison','NOT RUN')
        identifier=hashlib.sha256(row['relative_path'].encode('utf8')).hexdigest()
        retry,retry_path=retries.get(sha,({},None))
        check,check_path=checks.get(sha,({},None))
        db.execute('INSERT INTO books VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)',(identifier,row['relative_path'],row['size'],sha,row['metadata_json'],row['historical_json'],row['error'],status,xsd,comparison,path,json.dumps(retry,ensure_ascii=False),retry_path,check.get('status'),check_path))
        path_counts[status]+=1
        if sha:unique_status[sha]=status
        metadata=json.loads(row['metadata_json'])
        rows.append({'id':identifier,'relative_path':row['relative_path'],'size':row['size'],'sha256':sha,
                     'epub_version':metadata.get('version'),'language':' | '.join(metadata.get('language',[])),
                     'title':' | '.join(metadata.get('title',[])),'authors':' | '.join(metadata.get('creator',[])),
                     'spine':metadata.get('spine'),'images':metadata.get('images'),
                     'features':' | '.join(metadata.get('features',[])),
                     'inspection_error':metadata.get('inspection_error'),
                     'history_matched':'yes' if json.loads(row['historical_json']) else 'no',
                     'status':status,'xsd':xsd,'comparison':comparison,'result_path':path,
                     'retry_status':retry.get('status'),'retry_xsd':retry.get('xsd'),
                     'retry_comparison':retry.get('comparison'),'retry_path':retry_path,
                     'xsd_recheck':check.get('status'),'xsd_recheck_path':check_path})
    db.commit();db.close();source.close();containers.close()
    with csv_path.open('x',encoding='utf-8-sig',newline='') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(rows[0]) if rows else ['id']);writer.writeheader();writer.writerows(rows)
    print(json.dumps({'paths':len(rows),'unique':len(unique_status),'by_path':path_counts,'by_unique':Counter(unique_status.values()),'conversion_results':len(conversions)},ensure_ascii=False))


if __name__=='__main__':main()
