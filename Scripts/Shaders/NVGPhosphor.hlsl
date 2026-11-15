// SceneColor is pre-bloom HDR. Keep true black at zero: a tube needs photons.
float3 signal=max(SceneColor.rgb,0.0);
float nearFocus=1.0-smoothstep(35.0,100.0,SceneDepth.r);
// Mild near-focus loss; five taps preserve the distant image and soften hands/near walls.
if(nearFocus>0.001)
{
 float2 uv=GetDefaultSceneTextureUV(Parameters,14);
 float2 pixel=InputInvSize.xy*1.5;
 float3 soft=signal*0.4;
 soft+=SceneTextureLookup(uv+float2(pixel.x,0),14,false).rgb*0.15;
 soft+=SceneTextureLookup(uv-float2(pixel.x,0),14,false).rgb*0.15;
 soft+=SceneTextureLookup(uv+float2(0,pixel.y),14,false).rgb*0.15;
 soft+=SceneTextureLookup(uv-float2(0,pixel.y),14,false).rgb*0.15;
 signal=lerp(signal,soft,nearFocus*0.65);
}
float luma=dot(signal,float3(0.2126,0.7152,0.0722));
float vis=luma/(luma+0.20);
float blow=smoothstep(0.55,1.80,luma);
// Rational shoulder approaches 1.8 without hard-clipping bright surface detail.
float shoulder=1.8*luma/(luma+1.8);
vis=lerp(vis,shoulder,blow*0.65);
vis=lerp(vis,0.75*vis/(0.35+vis),nearFocus*0.45);
// Near objects are out of tube focus: keep foreground hands/metal from flaring.
vis*=lerp(1.0,0.60,nearFocus);
float side=smoothstep(0.27,0.43,abs(ViewportUV.x-0.5))*saturate(Quad);
float t=floor(TubeTime*24.0);
float n=frac(sin(dot(Parameters.SvPosition.xy+t*17.0,float2(12.9898,78.233)))*43758.5453);
float grain=lerp(0.30,0.05,saturate(vis))*(1.0+(1.0-saturate(Battery)))*lerp(1.0,1.15,side);
vis*=1.0+(n-0.5)*grain;
vis=min(vis,1.8)*lerp(1.0,0.85,side);
return PhosphorTint.rgb*max(vis,0.0);
