#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Validate common authored narrative sources; neither world engine exists yet."""
from __future__ import annotations
import argparse
import re
import tomllib
from pathlib import Path

TRACKS={'metroid','castlevania','shared'}
WORLDS={'zero_mission','aria','interzone'}
STEP_TYPES={'dialogue','move','animation','wait','sound','music','portal','set_flag','choice','camera'}
ID=re.compile(r'^[a-z][a-z0-9_.:-]{0,95}$')


def validate_timeline(data:dict)->int:
    if data.get('schema')!='metroid-vania-timeline' or data.get('version')!=1:raise ValueError('timeline schema/version mismatch')
    events=data.get('events')
    if not isinstance(events,list) or not events:raise ValueError('timeline needs events')
    ids=set(); producers={}
    for e in events:
        ident=e.get('id')
        if not isinstance(ident,str) or not ID.fullmatch(ident) or ident in ids:raise ValueError('duplicate/invalid event ID')
        ids.add(ident)
        if e.get('track') not in TRACKS or e.get('world') not in WORLDS:raise ValueError(f'{ident}: invalid track/world')
        if not isinstance(e.get('title'),str) or not e['title']:raise ValueError(f'{ident}: missing title')
        if not isinstance(e.get('min_order'),int) or not isinstance(e.get('max_order'),int) or e['min_order']>e['max_order']:raise ValueError(f'{ident}: order range')
        for flag in e.get('sets',[]):
            if flag in producers:raise ValueError(f'duplicate flag producer: {flag}')
            producers[flag]=ident
    deps={}
    for e in events:
        deps[e['id']]=set()
        for flag in e.get('requires',[]):
            if flag not in producers:raise ValueError(f'{e["id"]}: requirement has no producer {flag}')
            deps[e['id']].add(producers[flag])
    visit=set();done=set()
    def walk(node):
        if node in visit:raise ValueError(f'timeline dependency cycle: {node}')
        if node in done:return
        visit.add(node)
        for dep in deps[node]:walk(dep)
        visit.remove(node);done.add(node)
    for e in deps:walk(e)
    return len(events)


def validate_scene(data:dict)->int:
    if data.get('schema')!='metroid-vania-cutscene' or data.get('version')!=1:raise ValueError('cutscene schema/version mismatch')
    if not isinstance(data.get('id'),str) or not ID.fullmatch(data['id']):raise ValueError('invalid cutscene ID')
    if data.get('world') not in WORLDS:raise ValueError('invalid scene target world')
    actors={a['id'] for a in data.get('actors',[]) if isinstance(a,dict) and isinstance(a.get('id'),str)}
    steps=data.get('steps')
    if not isinstance(steps,list) or not steps:raise ValueError('cutscene requires steps')
    for i,step in enumerate(steps):
        if not isinstance(step,dict) or step.get('type') not in STEP_TYPES:raise ValueError(f'step {i}: unknown action')
        if step['type'] in {'move','animation','dialogue'} and step.get('actor') not in actors:raise ValueError(f'step {i}: unknown actor')
        if step['type']=='dialogue' and not isinstance(step.get('text'),str):raise ValueError(f'step {i}: missing text')
        if step['type']=='move' and (not isinstance(step.get('x'),int) or not isinstance(step.get('y'),int)):raise ValueError(f'step {i}: invalid destination')
        if step['type']=='set_flag' and not isinstance(step.get('flag'),str):raise ValueError(f'step {i}: invalid flag')
    return len(steps)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--kind',choices=('timeline','cutscene'),required=True)
    ap.add_argument('path',type=Path)
    a=ap.parse_args()
    try:
        data=tomllib.loads(a.path.read_text(encoding='utf-8'))
        n=validate_timeline(data) if a.kind=='timeline' else validate_scene(data)
    except (OSError,ValueError,KeyError,TypeError,tomllib.TOMLDecodeError) as exc:
        ap.error(str(exc))
    print(f'Validated {a.kind}: {n} records')

if __name__=='__main__':main()
