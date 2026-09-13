L=170; r=24
def piece(top, right, bottom, left):
    # each side: 'out', 'in', or None ; path starts at 0,0, clockwise
    def arc(dx,dy,mode): return f"a{r} {r} 0 1 {1 if mode=='out' else 0} {dx} {dy}"
    p=["M0 0"]
    p.append(f"H{L/2-r} " + (arc(2*r,0,top) if top else f"H{L/2+r}") + f" H{L}")
    p.append(f"V{L/2-r} " + (arc(0,2*r,right) if right else f"V{L/2+r}") + f" V{L}")
    p.append(f"H{L/2+r} " + (arc(-2*r,0,bottom) if bottom else f"H{L/2-r}") + " H0")
    p.append(f"V{L/2+r} " + (arc(0,-2*r,left) if left else f"V{L/2-r}") + " Z")
    return " ".join(p)
OL='stroke="#141b4d" stroke-width="12" stroke-linejoin="round" stroke-linecap="round"'
Y=piece(None,'out','out',None)      # yellow: knob right + bottom-ish? keep bottom flat
Y=piece(None,'out',None,None)
C=piece('in','out',None,'in')       # cyan: socket left (receives yellow), socket top (receives coral), knob right
R=piece(None,None,'out',None)       # coral: knob bottom (fits cyan top socket)
def face(path, fill, dark, tx, ty, extra=""):
    return f'''<g transform="translate({tx} {ty})">
  <path d="{path}" transform="translate(8 18)" fill="{dark}" {OL}/>
  <path d="{path}" fill="{fill}" {OL}/>
  {extra}
</g>'''
toggle='''<rect x="34" y="66" width="104" height="46" rx="23" fill="#2fbfd0" stroke="#141b4d" stroke-width="10"/>
  <circle cx="115" cy="89" r="17" fill="#ffffff" stroke="#141b4d" stroke-width="8"/>
  <rect x="0" y="0" width="0" height="0"/>'''
sym='''<g transform="translate(6 6)" fill="none" stroke="#0e7f8c" stroke-width="9" stroke-linecap="round" stroke-linejoin="round">
    <path d="M42 71 l14 -28 14 28 z"/>
    <circle cx="114" cy="56" r="15"/>
    <path d="M42 100 l28 28 M70 100 l-28 28"/>
    <rect x="100" y="100" width="28" height="28" rx="4"/>
  </g>'''
def scene(rot):
    g_open = f'<g transform="translate(256 256) scale(0.86) translate(-256 -256) rotate({rot} 256 300)">' if rot else '<g transform="translate(256 256) scale(0.86) translate(-256 -256)">'
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">
<rect width="512" height="512" fill="#002887"/>
{g_open}
<ellipse cx="262" cy="440" rx="212" ry="26" fill="#001a5c"/>
{face(C, "#2fbfd0", "#1a8a9c", 246, 232, sym)}
{face(Y, "#f5b731", "#c9741f", 76, 232, toggle)}
<g transform="translate(286 30) rotate(22) scale(0.72)">
  <path d="{R}" transform="translate(8 18)" fill="#b23d36" {OL}/>
  <path d="{R}" fill="#e8574d" {OL}/>
</g>
<g stroke="#ffd166" stroke-width="10" stroke-linecap="round">
  <path d="M250 84 l24 -10"/><path d="M256 118 l26 -6"/>
</g>
</g>
</svg>'''
open('v2a_flat.svg','w').write(scene(0))
open('v2b_tilt.svg','w').write(scene(-7))
