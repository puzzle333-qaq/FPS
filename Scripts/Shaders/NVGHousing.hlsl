float2 p=float2(UV.x*Aspect,UV.y);
float cy=0.475;
float d=min(length(p-float2(0.355*Aspect,cy)),length(p-float2(0.645*Aspect,cy)))-0.34;
if(Quad>0.5)
{
 d=min(d,length(p-float2(0.13*Aspect,cy))-0.30);
 d=min(d,length(p-float2(0.87*Aspect,cy))-0.30);
}
// Fully opaque outside the tubes; only the inside edge is softened.
return smoothstep(-2.5/max(ScreenHeight,1.0),0.0,d);
