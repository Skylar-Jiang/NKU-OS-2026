"""Render member B Markdown into the existing DOCX package, preserving image bytes."""
from pathlib import Path
import copy, hashlib, re, struct, zipfile
import xml.etree.ElementTree as E
ROOT = Path(__file__).resolve().parents[3]
MD = ROOT / 'report/member-b/实验二_乙.md'
DOC = MD.with_name('实验二_乙_最终版.docx')
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
R='http://schemas.openxmlformats.org/officeDocument/2006/relationships'
A='http://schemas.openxmlformats.org/drawingml/2006/main'
WP='http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing'
NS={'w':W,'r':R,'a':A,'wp':WP}
for p,u in NS.items(): E.register_namespace(p,u)
def el(tag,attrs=None): return E.Element('{'+W+'}'+tag, {'{'+W+'}'+k:str(v) for k,v in (attrs or {}).items()})
with zipfile.ZipFile(DOC) as z: files={n:z.read(n) for n in z.namelist()}
doc=E.fromstring(files['word/document.xml']); body=doc.find('w:body',NS)
sect=copy.deepcopy(body.find('w:sectPr',NS)); template=copy.deepcopy(doc.find('.//w:drawing',NS))
rels=E.fromstring(files['word/_rels/document.xml.rels']); images={}
for r in rels:
 target=r.attrib['Target']
 if 'media/' in target:
  data=files['word/'+target];images[hashlib.sha256(data).digest()]=r.attrib['Id']
# Add newly captured screenshot assets to the existing package.
for ref in re.findall(r'!\[[^\]]*\]\(([^)]+)\)', MD.read_text()):
    data=(MD.parent/ref).resolve().read_bytes(); digest=hashlib.sha256(data).digest()
    if digest not in images:
        idx=len(images)+1; rid='rIdNew'+str(idx); target='media/member-b-new-'+str(idx)+'.png'
        files['word/'+target]=data
        E.SubElement(rels,'{http://schemas.openxmlformats.org/package/2006/relationships}Relationship',{'Id':rid,'Type':R+'/image','Target':target})
        images[digest]=rid
files['word/_rels/document.xml.rels']=E.tostring(rels,encoding='utf-8',xml_declaration=True)
body.clear()
for node in doc.iter():
    node.attrib.pop('{http://schemas.openxmlformats.org/markup-compatibility/2006}Ignorable',None)
def paragraph(text,size=24,bold=False,keep=False):
 p=el('p');pr=el('pPr');pr.append(el('spacing',{'after':100,'line':300,'lineRule':'auto'}))
 if keep: pr.append(el('keepNext'))
 p.append(pr);r=el('r');rp=el('rPr');rp.append(el('rFonts',{'ascii':'SimSun','hAnsi':'SimSun','eastAsia':'宋体'}));rp.append(el('sz',{'val':size}));rp.append(el('color',{'val':'000000'}))
 if bold:rp.append(el('b'))
 r.append(rp);t=el('t');t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=text;r.append(t);p.append(r);body.append(p);return p
counter=1
for line in MD.read_text().splitlines():
 if not line or line.startswith('```'):continue
 m=re.match(r'!\[(.*?)\]\((.*?)\)',line)
 if m:
  data=(MD.parent/m[2]).resolve().read_bytes();rid=images[hashlib.sha256(data).digest()];width,height=struct.unpack('>II',data[16:24]);cx=min(5500000,width*9525);cy=int(cx*height/width)
  p=el('p');pr=el('pPr');pr.append(el('keepNext'));p.append(pr);run=el('r');drawing=copy.deepcopy(template)
  drawing.find('.//a:blip',NS).set('{'+R+'}embed',rid)
  for node in drawing.iter():
   if node.tag.endswith('}extent') or node.tag.endswith('}ext'):
    if 'cx' in node.attrib: node.set('cx',str(cx));node.set('cy',str(cy))
   if node.tag.endswith('}docPr'):node.set('id',str(counter));node.set('name','Evidence '+str(counter))
  counter+=1;run.append(drawing);p.append(run);body.append(p);paragraph(m[1]);continue
 if re.match(r'\|[ -]+\|',line):continue
 heading=line.startswith('#');text=re.sub(r'^#+\s*|^>\s*|^-\s*','',line)
 text=re.sub(r'\[([^\]]+)\]\(([^)]+)\)',r'\1（\2）',text).replace('`','')
 paragraph(text,28 if heading else 24,heading,heading)
body.append(sect)
files['word/document.xml']=E.tostring(doc,encoding='utf-8',xml_declaration=True)
# Keep a minimal OPC package to avoid inherited Word extensions.
files = {n:b for n,b in files.items() if n in ('word/document.xml','word/_rels/document.xml.rels') or n.startswith('word/media/')}
rels = E.fromstring(files['word/_rels/document.xml.rels'])
for rel in list(rels):
    if '/image' not in rel.attrib.get('Type',''): rels.remove(rel)
files['word/_rels/document.xml.rels'] = E.tostring(rels,encoding='utf-8',xml_declaration=True)
files['_rels/.rels'] = b'''<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/></Relationships>'''
files['[Content_Types].xml'] = b'''<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Default Extension="png" ContentType="image/png"/><Default Extension="jpeg" ContentType="image/jpeg"/><Default Extension="jpg" ContentType="image/jpeg"/><Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/></Types>'''
with zipfile.ZipFile(DOC,'w',zipfile.ZIP_DEFLATED) as z:
 for n,b in files.items():z.writestr(n,b)
print(DOC)
