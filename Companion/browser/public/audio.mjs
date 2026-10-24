import {analyze,roles} from './core.mjs';
export class KitchenAudio {
  context(){ return this.ctx??=new AudioContext(); }
  async unlock(){const ctx=this.context();await ctx.resume();return ctx;}
  async import(file){
    if(file.size>50*1024*1024)throw new Error(`${file.name}: exceeds the 50 MB per-file limit.`);
    const buffer=await this.context().decodeAudioData(await file.arrayBuffer()).catch(()=>{throw new Error(`${file.name}: this browser cannot decode that audio format. Try PCM WAV.`);});
    if(buffer.duration>300)throw new Error(`${file.name}: use loops shorter than five minutes.`);
    const mono=new Float32Array(buffer.length);
    for(let ch=0;ch<buffer.numberOfChannels;ch++){const data=buffer.getChannelData(ch);for(let i=0;i<mono.length;i++)mono[i]+=data[i]/buffer.numberOfChannels;}
    return {id:`${file.name}:${file.size}:${file.lastModified}`,name:file.name,buffer,...analyze(mono,buffer.sampleRate,file.name),source:'Local file'};
  }
  demo(){
    const ctx=this.context(),rate=ctx.sampleRate,seconds=8,pantry=[];
    for(let role=0;role<5;role++)for(let variant=0;variant<2;variant++){
      const buffer=ctx.createBuffer(1,rate*seconds,rate),data=buffer.getChannelData(0);let rand=100+role*7+variant;
      for(let i=0;i<data.length;i++){
        const t=i/rate,beat=t%.5;rand=(Math.imul(rand,1664525)+1013904223)>>>0;const noise=(rand/4294967296)*2-1;
        if(role===0)data[i]=.32*Math.sin(2*Math.PI*(48*beat+3*(1-Math.exp(-30*beat))))*Math.exp(-20*beat)+noise*.04*Math.exp(-80*(t%.25));
        if(role===1)data[i]=.2*Math.sin(2*Math.PI*(variant?65.406:130.813/2)*t)*Math.exp(-4*beat);
        if(role===2)data[i]=.08*(Math.sin(2*Math.PI*261.626*t)+Math.sin(2*Math.PI*311.127*t)+Math.sin(2*Math.PI*(variant?466.164:391.995)*t))*Math.exp(-3*(t%2));
        if(role===3)data[i]=.045*(Math.sin(2*Math.PI*523.252*t)+Math.sin(2*Math.PI*784*t))*(.7+.3*Math.sin(2*Math.PI*.25*t));
        if(role===4)data[i]=.08*Math.sin(2*Math.PI*(variant?391.995:311.127)*t)*Math.sin(2*Math.PI*2*t)*Math.exp(-6*(t%1));
      }
      pantry.push({id:`demo-${role}-${variant}`,name:[['Cast iron kick','Kitchen break'],['Deep roux','Low simmer'],['Velvet keys','Sunday chords'],['Night air','Warm steam'],['Synthetic call','Synthetic response']][role][variant],buffer,...analyze(data,rate),bpm:120,bpmSource:'generated',role:roles[role],source:'Original synthetic demo'});
    }return pantry;
  }
  graph(ctx,layers,bpm,master,destination,when,duration){
    const nodes=[],solo=layers.some(l=>l.solo);
    layers.forEach((l,i)=>{
      if(!l.sample)return;
      const source=ctx.createBufferSource(),filter=ctx.createBiquadFilter(),pan=ctx.createStereoPanner(),gain=ctx.createGain();
      source.buffer=l.sample.buffer;source.loop=true;source.playbackRate.value=bpm/(l.sample.bpm||bpm);
      filter.type='lowpass';filter.frequency.value=Math.min(ctx.sampleRate*.45,180*Math.pow(100,l.filter));pan.pan.value=l.pan;
      gain.gain.value=.3*master*l.gain*(solo&&!l.solo?0:1);
      source.connect(filter).connect(pan).connect(gain).connect(destination);source.start(when);if(duration)source.stop(when+duration);
      nodes.push({source,filter,pan,gain,index:i});
    });return nodes;
  }
  async play(layers,bpm,master){const ctx=await this.unlock();this.stop();this.nodes=this.graph(ctx,layers,bpm,master,ctx.destination,ctx.currentTime+.05);this.playing=true;}
  update(layers,bpm,master){if(!this.playing)return;const ctx=this.ctx,solo=layers.some(l=>l.solo);
    for(const node of this.nodes){const l=layers[node.index];node.source.playbackRate.setTargetAtTime(bpm/(l.sample.bpm||bpm),ctx.currentTime,.02);node.filter.frequency.setTargetAtTime(Math.min(ctx.sampleRate*.45,180*Math.pow(100,l.filter)),ctx.currentTime,.02);node.pan.pan.setTargetAtTime(l.pan,ctx.currentTime,.02);node.gain.gain.setTargetAtTime(.3*master*l.gain*(solo&&!l.solo?0:1),ctx.currentTime,.02);}
  }
  stop(){for(const n of this.nodes||[]){try{n.source.stop();}catch{}for(const node of [n.source,n.filter,n.pan,n.gain])node.disconnect();}this.nodes=[];this.playing=false;}
  async render(layers,bpm,master,bars){const seconds=bars*4*60/bpm,rate=44100,ctx=new OfflineAudioContext(2,Math.ceil(seconds*rate),rate);this.graph(ctx,layers,bpm,master,ctx.destination,0,seconds);return ctx.startRendering();}
}
