"""Find REVIEW candidates where unique blocks moved into FB2 notes."""
import argparse
from collections import Counter,defaultdict
import json
from pathlib import Path


def main():
    p=argparse.ArgumentParser();p.add_argument('--run',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    if args.output.exists():p.error('output exists')
    cases=[];errors=0
    for result_path in args.run.rglob('result.json'):
        comparison=result_path.with_name('comparison.json')
        if not comparison.exists():errors+=1;continue
        result=json.loads(result_path.read_text(encoding='utf8'))
        report=json.loads(comparison.read_text(encoding='utf8'))
        source,target=report['epub']['blocks'],report['fb2']['blocks']
        left=Counter(item['normalized_sha256'] for item in source)
        right=Counter(item['normalized_sha256'] for item in target)
        targets={item['normalized_sha256']:item['body'] for item in target if right[item['normalized_sha256']]==1}
        by_document=Counter()
        for item in source:
            digest=item['normalized_sha256'];name=item['document']
            if not item.get('spine') or 'note' in Path(name).stem.lower():continue
            if left[digest]==1 and targets.get(digest) in ('notes','comments'):
                by_document[name]+=1
        if by_document:
            cases.append({'sha256':result['source_sha256'],'path':result['source_relative_path'],
                          'candidate_count':sum(by_document.values()),'documents':dict(by_document)})
    cases.sort(key=lambda row:row['candidate_count'],reverse=True)
    args.output.write_text(json.dumps({'status':'REVIEW','cases':cases,'no_comparison':errors},ensure_ascii=False,indent=2),encoding='utf8')
    print('review cases',len(cases),'missing comparison',errors)
    for case in cases[:10]: print(case['candidate_count'],case['path'])


if __name__=='__main__':main()
