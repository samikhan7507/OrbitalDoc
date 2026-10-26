// Orbital Dock 3D - all GLSL shaders (embedded so the web build needs no file loading).
// Everything visual (stars, Milky Way, Earth, Moon, hull panels) is procedural.
// Author: Sami Khan
#pragma once

namespace Shaders {

// ---------------------------------------------------------------- shared GLSL helpers
static const char* COMMON = R"GLSL(
float hash13(vec3 p){ p=fract(p*0.1031); p+=dot(p,p.zyx+31.32); return fract((p.x+p.y)*p.z); }
vec3 hash33(vec3 p){ p=fract(p*vec3(.1031,.1030,.0973)); p+=dot(p,p.yxz+33.33); return fract((p.xxy+p.yxx)*p.zyx); }
float vnoise(vec3 x){
  vec3 i=floor(x), f=fract(x); f=f*f*(3.0-2.0*f);
  return mix(mix(mix(hash13(i),hash13(i+vec3(1,0,0)),f.x),mix(hash13(i+vec3(0,1,0)),hash13(i+vec3(1,1,0)),f.x),f.y),
             mix(mix(hash13(i+vec3(0,0,1)),hash13(i+vec3(1,0,1)),f.x),mix(hash13(i+vec3(0,1,1)),hash13(i+vec3(1,1,1)),f.x),f.y),f.z);
}
float fbm(vec3 p){ float a=0.5,s=0.0; for(int i=0;i<5;i++){ s+=a*vnoise(p); p=p*2.03+vec3(1.7,9.2,3.1); a*=0.5; } return s; }
vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }
vec3 finalColor(vec3 c){ return pow(aces(c), vec3(1.0/2.2)); }
)GLSL";

// ---------------------------------------------------------------- generic vertex shader
static const char* VERT = R"GLSL(
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
uniform mat4 uModel;
uniform mat4 uVP;
uniform mat3 uNormalMat;
out vec3 vWorld; out vec3 vNormal; out vec3 vObj; out vec3 vObjN; out vec2 vUV;
void main(){
  vec4 w = uModel * vec4(aPos,1.0);
  vWorld = w.xyz;
  vNormal = normalize(uNormalMat * aNormal);
  vObj = aPos; vObjN = aNormal; vUV = aUV;
  gl_Position = uVP * w;
}
)GLSL";

// ---------------------------------------------------------------- sky: stars + Milky Way + nebulae + Sun
static const char* SKY_VERT = R"GLSL(
out vec2 vNdc;
void main(){
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  vNdc = p * 2.0 - 1.0;
  gl_Position = vec4(vNdc, 1.0, 1.0);
}
)GLSL";

static const char* SKY_FRAG = R"GLSL(
in vec2 vNdc;
uniform mat4 uInvVP;
uniform vec3 uSunDir;
uniform float uTime;
out vec4 frag;

vec3 starLayer(vec3 dir, float scale, float density, float size, float bright){
  vec3 p = dir*scale; vec3 cell=floor(p); vec3 f=fract(p);
  float h = hash13(cell);
  if(h > density) return vec3(0.0);
  vec3 o = hash33(cell+17.0)*0.7+0.15;
  float px = length(fwidth(p));
  float sz = max(size, 1.1*px);
  float d = length(f-o);
  float s = smoothstep(sz, 0.0, d);
  float tw = 0.8+0.2*sin(uTime*(1.0+h*6.0)+h*91.0);
  float k = hash13(cell+3.7);
  vec3 col = mix(vec3(1.0,0.72,0.5), vec3(0.62,0.78,1.0), k);
  float mag = pow(hash13(cell+8.1),4.0);
  return col * s*s * bright * (0.12+3.2*mag) * tw * clamp(size/sz,0.25,1.0);
}

void main(){
  vec4 w = uInvVP*vec4(vNdc,1.0,1.0);
  vec3 dir = normalize(w.xyz/w.w);

  // ---- Milky Way: a bright band around a tilted great circle with dust lanes
  vec3 gn = normalize(vec3(0.45,0.78,0.2));
  float b = dot(dir,gn);
  float band = exp(-b*b/0.010);
  float wide = exp(-b*b/0.09);
  float n1 = fbm(dir*4.0+vec3(2.0));
  float n2 = fbm(dir*9.0);
  float dust = smoothstep(0.42,0.68,fbm(dir*6.5+vec3(7.0)))*band;
  float grain = 0.55+0.9*vnoise(dir*260.0)*vnoise(dir*130.0);
  float mw = (band*(0.10+1.7*n1*n1)*grain + wide*0.05) * (1.0-0.85*dust);
  vec3 gc = normalize(cross(gn,vec3(0.0,0.0,1.0)));
  float bulge = pow(max(dot(dir,gc),0.0),5.0)*band;
  vec3 mwCol = mix(vec3(0.36,0.48,0.95), vec3(1.0,0.76,0.52), clamp(bulge*1.8+n2*0.3,0.0,1.0));
  vec3 col = vec3(0.0015,0.0022,0.005);
  col += mwCol*mw*0.42 + vec3(1.0,0.82,0.6)*bulge*0.30*(1.0-0.7*dust);

  // ---- coloured nebula clouds
  float nb = smoothstep(0.52,0.85,fbm(dir*2.2+vec3(11.0)))*(0.2+wide);
  vec3 nebCol = mix(vec3(0.55,0.1,0.55), vec3(0.08,0.38,0.75), fbm(dir*3.0+vec3(5.0)));
  col += nebCol*nb*0.13;

  // ---- stars, denser inside the galactic band
  float dens = 1.0+2.2*band;
  vec3 stars = starLayer(dir, 40.0, 0.06*dens, 0.05, 1.6) + starLayer(dir, 90.0, 0.05*dens, 0.06, 1.2)
             + starLayer(dir, 200.0, 0.04*dens, 0.07, 0.9) + starLayer(dir, 450.0, 0.035*dens, 0.08, 0.7);
  col += stars;

  // ---- the Sun: disc + halo
  float sd = dot(dir,uSunDir);
  float disc = smoothstep(0.99955,0.99972,sd)*45.0;
  float halo = pow(max(sd,0.0),700.0)*7.0 + pow(max(sd,0.0),50.0)*0.40 + pow(max(sd,0.0),6.0)*0.03;
  col += vec3(1.0,0.93,0.8)*(disc+halo);
  float glow = clamp(disc*0.05 + halo*0.28 + dot(stars,vec3(0.33))*0.10, 0.0, 1.0);
  frag = vec4(finalColor(col),glow);
}
)GLSL";

// ---------------------------------------------------------------- Earth
static const char* EARTH_FRAG = R"GLSL(
in vec3 vWorld; in vec3 vNormal; in vec3 vObj; in vec3 vObjN; in vec2 vUV;
uniform vec3 uSunDir; uniform vec3 uCamPos; uniform float uTime;
out vec4 frag;
void main(){
  vec3 n = normalize(vObj), N = normalize(vNormal), V = normalize(uCamPos-vWorld), L = normalize(uSunDir);
  float lat = abs(n.y);
  float h = fbm(n*2.6+vec3(4.0)) + 0.26*(fbm(n*12.0)-0.5) + 0.10*(fbm(n*45.0)-0.5) + 0.04*(vnoise(n*160.0)-0.5);
  float land = smoothstep(0.513,0.521,h);
  float depth = smoothstep(0.34,0.52,h);
  vec3 ocean = mix(vec3(0.004,0.025,0.11), vec3(0.03,0.2,0.38), depth);
  float moist = fbm(n*5.0+vec3(9.0))*0.75+0.25*fbm(n*30.0);
  float arid = clamp((0.55-moist)*3.0 + (0.42-abs(lat-0.30))*2.4, 0.0, 1.0);
  vec3 green = mix(vec3(0.03,0.12,0.03), vec3(0.09,0.25,0.06), moist);
  vec3 landc = mix(green, vec3(0.52,0.41,0.22), arid)*(0.75+0.5*fbm(n*70.0));
  landc = mix(landc, vec3(0.30,0.27,0.24), smoothstep(0.6,0.72,h));
  float ice = smoothstep(0.86,0.92,lat+0.06*fbm(n*14.0));
  float snow = clamp(smoothstep(0.80,0.9,h)+ice,0.0,1.0);
  landc = mix(landc, vec3(0.9), snow);
  vec3 surf = mix(mix(ocean,landc,land), vec3(0.85,0.9,0.95), ice);
  float landMask = land*(1.0-ice);

  vec3 cp = n*13.0 + vec3(uTime*0.01, uTime*0.004, 0.0);
  vec3 warp = vec3(fbm(cp*0.6+vec3(3.0)), fbm(cp*0.6+vec3(8.0)), fbm(cp*0.6+vec3(1.0)))-0.5;
  float cl = fbm(cp+warp*1.6)*0.8 + 0.30*fbm(cp*3.1+vec3(4.0)) + 0.10*vnoise(cp*14.0);
  float cloud = smoothstep(0.56,0.74,cl)*0.9;

  float diff = dot(N,L);
  float lam = max(diff,0.0);
  vec3 sun = vec3(1.0,0.96,0.88)*1.9;
  float spec = pow(max(dot(N,normalize(L+V)),0.0),110.0)*(1.0-land)*(1.0-cloud)*(1.0-ice);
  vec3 color = surf*(sun*lam*(1.0-0.35*cloud) + vec3(0.008,0.012,0.025)) + spec*sun*0.5*lam;
  vec3 cloudCol = vec3(0.88,0.9,0.94)*(sun*lam*0.46 + vec3(0.010,0.014,0.026));
  color = mix(color, cloudCol, cloud);
  float tw = exp(-pow(diff/0.15,2.0));
  color += vec3(1.0,0.38,0.12)*tw*(0.12+cloud*0.5);

  float night = 1.0-smoothstep(-0.10,0.06,diff);
  float cities = smoothstep(0.70,0.85,fbm(n*45.0+vec3(3.0)))*smoothstep(0.35,0.55,fbm(n*7.0+vec3(1.0)))*landMask*(1.0-cloud*0.85);
  color += vec3(1.0,0.68,0.32)*cities*night*1.8;

  float fres = pow(1.0-max(dot(N,V),0.0),3.2);
  color += vec3(0.22,0.48,1.0)*fres*clamp(diff*1.3+0.25,0.0,1.0)*0.85;
  frag = vec4(finalColor(color),0.0);
}
)GLSL";

// Atmosphere shell: analytic ray-through-shell thickness, additive blending.
static const char* ATMO_FRAG = R"GLSL(
in vec3 vWorld; in vec3 vNormal; in vec3 vObj; in vec3 vObjN; in vec2 vUV;
uniform vec3 uSunDir; uniform vec3 uCamPos; uniform vec3 uCenter; uniform float uRp; uniform float uRs;
out vec4 frag;
vec2 sph(vec3 ro, vec3 rd, float r){
  float b=dot(ro,rd); float c=dot(ro,ro)-r*r; float d=b*b-c;
  if(d<0.0) return vec2(-1.0);
  d=sqrt(d); return vec2(-b-d,-b+d);
}
void main(){
  vec3 ro = uCamPos-uCenter; vec3 rd = normalize(vWorld-uCamPos);
  vec2 a = sph(ro,rd,uRs);
  if(a.y<0.0) discard;
  float t0 = max(a.x,0.0), t1 = a.y;
  vec2 p = sph(ro,rd,uRp);
  float onDisc = 0.0;
  if(p.x>0.0){ t1 = min(t1,p.x); onDisc = 1.0; }
  float path = max(t1-t0,0.0);
  float maxPath = 2.0*sqrt(uRs*uRs-uRp*uRp);
  float dens = pow(clamp(path/maxPath,0.0,1.0),1.25) * mix(1.0,0.16,onDisc);
  vec3 mid = ro+rd*(0.5*(t0+t1));
  float s = dot(normalize(mid),uSunDir);
  float sunl = smoothstep(-0.30,0.35,s);
  float twi = exp(-pow(s/0.22,2.0));
  vec3 col = mix(vec3(0.16,0.42,1.0), vec3(1.0,0.5,0.2), twi*0.7)*sunl;
  vec3 outc = col*dens*1.5;
  frag = vec4(outc, clamp(dot(outc,vec3(0.33))*0.30,0.0,1.0));
}
)GLSL";

// ---------------------------------------------------------------- Moon (procedural craters with analytic normals)
static const char* MOON_FRAG = R"GLSL(
in vec3 vWorld; in vec3 vNormal; in vec3 vObj; in vec3 vObjN; in vec2 vUV;
uniform vec3 uSunObj; uniform vec3 uCamObj; uniform float uBump;
out vec4 frag;
vec4 craterField(vec3 p){
  vec3 ip=floor(p); float H=0.0; vec3 G=vec3(0.0);
  for(int i=-1;i<=1;i++) for(int j=-1;j<=1;j++) for(int k=-1;k<=1;k++){
    vec3 cell=ip+vec3(float(i),float(j),float(k));
    if(hash13(cell+9.1)<0.45) continue;
    vec3 c=cell+hash33(cell)*0.8+0.1;
    float r=0.18+0.32*hash13(cell+4.3);
    vec3 q=p-c; float dist=length(q); float d=dist/r;
    if(d>1.6||dist<1e-4) continue;
    float h=0.0, dh=0.0;
    if(d<1.0){ float s=1.0-d; h=-0.55*s*s*(3.0-2.0*s); dh=0.55*(6.0*s-6.0*s*s); }
    float x=(d-1.0)/0.22; float rim=0.28*exp(-x*x); h+=rim; dh+=rim*(-2.0*x/0.22);
    H+=h; G+=dh*q/(dist*r);
  }
  return vec4(H,G);
}
void main(){
  vec3 n=normalize(vObj); vec3 L=normalize(uSunObj);
  vec4 c1=craterField(n*7.0), c2=craterField(n*19.0+vec3(5.0)), c3=craterField(n*47.0+vec3(2.0));
  float H=c1.x+0.5*c2.x+0.25*c3.x;
  vec3 G=c1.yzw*7.0+c2.yzw*19.0*0.5+c3.yzw*47.0*0.25;
  G-=n*dot(G,n);
  vec3 N=normalize(n-uBump*G);
  float mare=smoothstep(0.48,0.62,fbm(n*1.6+vec3(3.0)));
  float alb=mix(0.62,0.20,mare)*(0.72+0.55*fbm(n*28.0))+0.10*max(H,0.0);
  vec3 base=vec3(alb)*vec3(1.0,0.98,0.94);
  float lam=max(dot(N,L),0.0)*smoothstep(0.0,0.07,dot(n,L));
  vec3 sun=vec3(1.0,0.96,0.88)*2.4;
  vec3 color=base*(sun*lam+vec3(0.012,0.017,0.03));
  frag=vec4(finalColor(color),0.0);
}
)GLSL";

// ---------------------------------------------------------------- spacecraft & station (lit, procedural hull)
static const char* LIT_FRAG = R"GLSL(
in vec3 vWorld; in vec3 vNormal; in vec3 vObj; in vec3 vObjN; in vec2 vUV;
uniform vec3 uSunDir; uniform vec3 uCamPos; uniform vec3 uEarthDir;
uniform vec3 uColor; uniform vec3 uEmissive; uniform vec3 uSize;
uniform float uMetal; uniform float uRough; uniform float uUnlit; uniform float uTime;
uniform int uPattern;
uniform sampler2D uShadow; uniform mat4 uLightVP; uniform float uShadowTexel; uniform float uShadowOn;
out vec4 frag;
// Sun shadow map lookup with 3x3 PCF; returns 1 = lit, 0 = shadowed.
float shadowFactor(vec3 wp, vec3 N, vec3 L){
  if(uShadowOn<0.5) return 1.0;
  vec4 lp = uLightVP*vec4(wp+N*0.07,1.0);
  vec3 c = lp.xyz/lp.w*0.5+0.5;
  if(c.x<0.0||c.x>1.0||c.y<0.0||c.y>1.0||c.z>1.0) return 1.0;
  float bias = 0.0005 + 0.0012*(1.0-clamp(dot(N,L),0.0,1.0));
  float s = 0.0;
  for(int i=-1;i<=1;i++) for(int j=-1;j<=1;j++){
    float d = texture(uShadow, c.xy+vec2(float(i),float(j))*uShadowTexel).r;
    s += (c.z-bias > d) ? 0.0 : 1.0;
  }
  return s/9.0;
}
void main(){
  vec3 N=normalize(vNormal), V=normalize(uCamPos-vWorld), L=normalize(uSunDir);
  vec3 base=uColor, emis=uEmissive; float rough=uRough, metal=uMetal;
  vec3 S=vObj*uSize; vec3 an=abs(vObjN);
  vec2 pc=(an.x>an.y&&an.x>an.z)?S.yz:((an.y>an.z)?S.xz:S.xy);
  if(uPattern==1){
    vec2 c=abs(fract(pc/1.6)-0.5); float e=smoothstep(0.455,0.5,max(c.x,c.y));
    base*=1.0-0.42*e; base*=0.86+0.28*vnoise(S*0.8);
  } else if(uPattern==2){
    vec2 g=vUV*vec2(14.0,6.0); vec2 cell=abs(fract(g)-0.5); float grid=smoothstep(0.44,0.5,max(cell.x,cell.y));
    float sheen=0.5+0.5*vnoise(vec3(g*0.3,1.0));
    base=mix(vec3(0.02,0.06,0.22)*(0.7+0.7*sheen), vec3(0.6,0.52,0.3), grid); metal=0.85; rough=0.22;
  } else if(uPattern==3){
    float u=vUV.x*180.0; float id=floor(u); float on=step(0.35,hash13(vec3(id,3.0,1.0)));
    float st=step(0.2,fract(u))*step(fract(u),0.8);
    float band=1.0-smoothstep(0.03,0.06,min(vUV.y,1.0-vUV.y));
    float band2=1.0-smoothstep(0.03,0.06,abs(vUV.y-0.5));
    emis+=vec3(1.0,0.82,0.5)*st*on*(band+band2*0.5)*2.2;
    base*=1.0-0.35*smoothstep(0.455,0.5,abs(fract(u/6.0)-0.5));
  } else if(uPattern==4){
    float u=vUV.x*24.0; float id=floor(u); float ph=fract(uTime*0.9-id/24.0);
    float blink=pow(1.0-ph,3.0);
    vec3 lc=mix(vec3(0.1,1.0,0.5),vec3(0.2,0.7,1.0),step(0.5,hash13(vec3(id,1.0,2.0))));
    emis+=lc*(0.3+3.2*blink)*step(0.2,fract(u))*step(fract(u),0.8);
  }
  else if(uPattern==5){   // rock: mottled grey-brown with noise bump
    float n=fbm(S*0.35+vec3(3.0));
    base=mix(vec3(0.20,0.17,0.15),vec3(0.50,0.45,0.40),n)*(0.75+0.5*vnoise(S*2.2));
    N=normalize(N+0.9*(vec3(vnoise(S*1.4),vnoise(S*1.4+5.0),vnoise(S*1.4+9.0))-0.5));
    metal=0.05; rough=0.9;
  }
  float glow = clamp(max(emis.r,max(emis.g,emis.b))*0.16,0.0,1.0);
  if(uUnlit>0.5){ frag=vec4(finalColor(emis),glow); return; }

  float diff=max(dot(N,L),0.0);
  vec3 H=normalize(L+V);
  float shin=mix(120.0,12.0,clamp(rough,0.0,1.0));
  float spec=pow(max(dot(N,H),0.0),shin)*(0.15+0.85*metal);
  vec3 sunC=vec3(1.0,0.95,0.86)*1.85;
  float earth=max(dot(N,uEarthDir),0.0);
  vec3 amb=vec3(0.02,0.025,0.04)+vec3(0.20,0.36,0.75)*earth*0.5;
  float rim=pow(1.0-max(dot(N,V),0.0),3.0);
  vec3 diffCol=base*(1.0-metal*0.65);
  vec3 specCol=mix(vec3(1.0),base,metal);
  float sh = shadowFactor(vWorld, normalize(vNormal), L);
  vec3 color=diffCol*(sunC*diff*sh+amb)+specCol*spec*sunC*diff*1.2*sh+vec3(0.25,0.45,0.9)*rim*earth*0.25+emis;
  frag=vec4(finalColor(color),glow);
}
)GLSL";


// ---------------------------------------------------------------- shadow pass (depth only)
static const char* DEPTH_FRAG = R"GLSL(
void main(){}
)GLSL";

// ---------------------------------------------------------------- particles (additive point sprites)
static const char* PART_VERT = R"GLSL(
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aCol;
layout(location=2) in float aSize;
uniform mat4 uVP;
uniform float uPxScale;
out vec4 vCol;
void main(){
  gl_Position = uVP*vec4(aPos,1.0);
  gl_PointSize = clamp(aSize*uPxScale/max(gl_Position.w,0.1), 1.0, 120.0);
  vCol = aCol;
}
)GLSL";
static const char* PART_FRAG = R"GLSL(
in vec4 vCol;
out vec4 frag;
void main(){
  vec2 d = gl_PointCoord*2.0-1.0; float r = dot(d,d);
  if(r>1.0) discard;
  float a = pow(1.0-r,2.0)*vCol.a;
  frag = vec4(vCol.rgb*a*0.85, a*0.22);
}
)GLSL";

// ---------------------------------------------------------------- post-processing: bloom + FXAA + vignette
static const char* POST_VERT = R"GLSL(
out vec2 vUV;
void main(){
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  vUV = p;
  gl_Position = vec4(p*2.0-1.0, 0.0, 1.0);
}
)GLSL";
// bright pass: only pixels flagged as "glowing" (alpha) leave the scene; 4-tap downsample
static const char* BRIGHT_FRAG = R"GLSL(
in vec2 vUV; uniform sampler2D uTex; uniform vec2 uTexel; out vec4 frag;
vec3 fetch(vec2 uv){ vec4 c=texture(uTex,uv); return c.rgb*c.a; }
void main(){
  vec3 b = fetch(vUV+uTexel*vec2(-0.5,-0.5))+fetch(vUV+uTexel*vec2(0.5,-0.5))+fetch(vUV+uTexel*vec2(-0.5,0.5))+fetch(vUV+uTexel*vec2(0.5,0.5));
  frag = vec4(b*0.25*1.4,1.0);
}
)GLSL";
static const char* BLUR_FRAG = R"GLSL(
in vec2 vUV; uniform sampler2D uTex; uniform vec2 uDir; out vec4 frag;
void main(){
  vec3 c = texture(uTex,vUV).rgb*0.2270270270;
  c += (texture(uTex,vUV+uDir*1.3846153846).rgb + texture(uTex,vUV-uDir*1.3846153846).rgb)*0.3162162162;
  c += (texture(uTex,vUV+uDir*3.2307692308).rgb + texture(uTex,vUV-uDir*3.2307692308).rgb)*0.0702702703;
  frag = vec4(c,1.0);
}
)GLSL";
static const char* COMPOSITE_FRAG = R"GLSL(
in vec2 vUV; uniform sampler2D uScene; uniform sampler2D uBloom1; uniform sampler2D uBloom2;
uniform vec2 uTexel; uniform float uBloom; uniform float uFxaa; out vec4 frag;
float luma(vec3 c){ return dot(c,vec3(0.299,0.587,0.114)); }
vec3 sceneAA(vec2 uv){
  vec3 rgbM = texture(uScene,uv).rgb;
  if(uFxaa<0.5) return rgbM;
  vec3 rgbNW = texture(uScene,uv+vec2(-1.0,-1.0)*uTexel).rgb, rgbNE = texture(uScene,uv+vec2(1.0,-1.0)*uTexel).rgb;
  vec3 rgbSW = texture(uScene,uv+vec2(-1.0,1.0)*uTexel).rgb,  rgbSE = texture(uScene,uv+vec2(1.0,1.0)*uTexel).rgb;
  float lM=luma(rgbM), lNW=luma(rgbNW), lNE=luma(rgbNE), lSW=luma(rgbSW), lSE=luma(rgbSE);
  float lMin=min(lM,min(min(lNW,lNE),min(lSW,lSE))), lMax=max(lM,max(max(lNW,lNE),max(lSW,lSE)));
  vec2 dir = vec2(-((lNW+lNE)-(lSW+lSE)), ((lNW+lSW)-(lNE+lSE)));
  float dirReduce = max((lNW+lNE+lSW+lSE)*0.25*0.125, 1.0/128.0);
  float rcp = 1.0/(min(abs(dir.x),abs(dir.y))+dirReduce);
  dir = clamp(dir*rcp, vec2(-8.0), vec2(8.0))*uTexel;
  vec3 a = 0.5*(texture(uScene,uv+dir*(1.0/3.0-0.5)).rgb + texture(uScene,uv+dir*(2.0/3.0-0.5)).rgb);
  vec3 b = a*0.5 + 0.25*(texture(uScene,uv+dir*-0.5).rgb + texture(uScene,uv+dir*0.5).rgb);
  float lB = luma(b);
  return (lB<lMin || lB>lMax) ? a : b;
}
void main(){
  vec3 c = sceneAA(vUV);
  vec3 bl = texture(uBloom1,vUV).rgb*0.85 + texture(uBloom2,vUV).rgb*1.15;
  c += bl*uBloom;
  vec2 q = vUV-0.5;
  c *= 1.0-0.55*dot(q,q);                       // gentle vignette
  frag = vec4(c,1.0);
}
)GLSL";

} // namespace Shaders
