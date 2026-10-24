export const roles=['drums','bass','keys','texture','vocals'];
export const ingredients=['Andouille','Shrimp','Okra','Filé','Bay leaf'];
export function analyze(samples,sampleRate,name='') {
  const length=Math.min(samples.length,Math.floor(sampleRate*20));
  let square=0,peak=0,cross=0; const hop=Math.max(1,Math.round(sampleRate/100)); const env=[];
  for(let start=0;start<length;start+=hop){let power=0,count=0;
    for(let n=start;n<Math.min(start+hop,length);n++){const x=samples[n];power+=x*x;count++;square+=x*x;peak=Math.max(peak,Math.abs(x));if(n&&x*samples[n-1]<0)cross++;}
    env.push(Math.sqrt(power/Math.max(1,count)));
  }
  const flux=env.map((x,i)=>i?Math.max(0,x-env[i-1]):0); let best=0,lagBest=0;
  for(let lag=33;lag<=100;lag++){let score=0,a=0,b=0;
    for(let i=lag;i<flux.length;i++){score+=flux[i]*flux[i-lag];a+=flux[i]**2;b+=flux[i-lag]**2;}
    score/=Math.sqrt(a*b)||1;if(score>best){best=score;lagBest=lag;}
  }
  const bpmMatch=name.match(/(?:^|[ _-])(\d{2,3})(?:[ _-]|bpm|\.)/i);const tag=bpmMatch?+bpmMatch[1]:0;
  const bpm=tag>=40&&tag<=240?tag:best>.15?Math.round(6000/lagBest):null;
  const rms=Math.sqrt(square/Math.max(1,length)); const zcr=cross/Math.max(1,length);
  let role=/drum|kick|snare|hat|perc/i.test(name)?'drums':/bass|sub|808/i.test(name)?'bass':/vocal|voice|vox|sing/i.test(name)?'vocals':/key|piano|chord|guitar|synth/i.test(name)?'keys':/pad|fx|texture|ambient/i.test(name)?'texture':zcr<.018?'bass':peak/Math.max(rms,.0001)>6?'drums':'texture';
  return {bpm,bpmSource:tag>=40&&tag<=240?'filename':'estimated',rms,peak,zcr,role,duration:samples.length/sampleRate};
}
export function compatibility(layers,targetBpm) {
  const active=layers.filter(l=>l.sample);if(!active.length)return {score:null,tempo:0,balance:0,energy:0,why:['Import sounds or load the demo pantry to start.']};
  const known=active.filter(l=>l.sample.bpm);const tempo=known.length?known.reduce((sum,l)=>sum+Math.max(0,1-Math.abs(Math.log2(targetBpm/l.sample.bpm))),0)/known.length:0;
  const balance=new Set(active.map(l=>l.sample.role)).size/5;
  const energies=active.map(l=>l.sample.rms*l.gain);const max=Math.max(...energies,.0001),min=Math.min(...energies);const energy=min/max;
  return {score:Math.round(100*(.5*tempo+.3*balance+.2*energy)),tempo:Math.round(tempo*100),balance:Math.round(balance*100),energy:Math.round(energy*100),why:[`${known.length}/${active.length} layers have tempo metadata or estimates.`,`${new Set(active.map(l=>l.sample.role)).size} distinct roles across ${active.length} layers.`,`Key and groove matching are not scored in this prototype.`]};
}
export function regenerate(layers,pantry,random=Math.random) {
  const used=new Set(layers.filter(l=>l.locked&&l.sample).map(l=>l.sample.id));
  return layers.map((layer,i)=>{
    if(layer.locked)return layer;
    const candidates=pantry.filter(s=>s.role===roles[i]&&!used.has(s.id));
    const pool=candidates.length?candidates:pantry.filter(s=>!used.has(s.id));
    const alternatives=pool.filter(s=>s.id!==layer.sample?.id);const choices=alternatives.length?alternatives:pool;
    const sample=choices.length?choices[Math.min(choices.length-1,Math.floor(random()*choices.length))]:null;
    if(sample)used.add(sample.id);return {...layer,sample};
  });
}
export function encodeWav(channels,sampleRate){
  const frames=channels[0].length,n=channels.length;const result=new ArrayBuffer(44+frames*n*2),v=new DataView(result);
  const str=(offset,s)=>[...s].forEach((c,i)=>v.setUint8(offset+i,c.charCodeAt(0)));
  str(0,'RIFF');v.setUint32(4,36+frames*n*2,true);str(8,'WAVE');str(12,'fmt ');v.setUint32(16,16,true);v.setUint16(20,1,true);v.setUint16(22,n,true);v.setUint32(24,sampleRate,true);v.setUint32(28,sampleRate*n*2,true);v.setUint16(32,n*2,true);v.setUint16(34,16,true);str(36,'data');v.setUint32(40,frames*n*2,true);
  let offset=44;for(let i=0;i<frames;i++)for(let ch=0;ch<n;ch++){const x=Math.max(-1,Math.min(1,channels[ch][i]));v.setInt16(offset,Math.round(x*(x<0?32768:32767)),true);offset+=2;}return result;
}
export function zipStore(files){
  const encoder=new TextEncoder();const chunks=[],directory=[];let offset=0;
  const crc32=bytes=>{let c=0xffffffff;for(const b of bytes){c^=b;for(let i=0;i<8;i++)c=(c>>>1)^((c&1)?0xedb88320:0);}return (c^0xffffffff)>>>0;};
  for(const {name,data} of files){const path=encoder.encode(name),bytes=new Uint8Array(data),crc=crc32(bytes);
    const header=new Uint8Array(30+path.length),h=new DataView(header.buffer);h.setUint32(0,0x04034b50,true);h.setUint16(4,20,true);h.setUint32(14,crc,true);h.setUint32(18,bytes.length,true);h.setUint32(22,bytes.length,true);h.setUint16(26,path.length,true);header.set(path,30);
    const central=new Uint8Array(46+path.length),c=new DataView(central.buffer);c.setUint32(0,0x02014b50,true);c.setUint16(4,20,true);c.setUint16(6,20,true);c.setUint32(16,crc,true);c.setUint32(20,bytes.length,true);c.setUint32(24,bytes.length,true);c.setUint16(28,path.length,true);c.setUint32(42,offset,true);central.set(path,46);
    chunks.push(header,bytes);directory.push(central);offset+=header.length+bytes.length;
  }
  const end=new Uint8Array(22),v=new DataView(end.buffer);v.setUint32(0,0x06054b50,true);v.setUint16(8,files.length,true);v.setUint16(10,files.length,true);v.setUint32(12,directory.reduce((s,b)=>s+b.length,0),true);v.setUint32(16,offset,true);
  return new Blob([...chunks,...directory,end],{type:'application/zip'});
}
