import test from 'node:test';
import assert from 'node:assert/strict';
import {analyze,compatibility,regenerate,encodeWav,zipStore,roles} from '../public/core.mjs';
test('audio analysis measures signal energy and tempo from periodic onsets',()=>{
 const rate=8000,data=new Float32Array(rate*8);for(let i=0;i<data.length;i++)data[i]=Math.sin(2*Math.PI*120*i/rate)*.3*Math.exp(-25*((i/rate)%.5));const a=analyze(data,rate,'kick.wav');assert.equal(a.bpm,120);assert.equal(a.role,'drums');assert.ok(a.rms>.02);assert.ok(a.peak>.2);const silence=analyze(new Float32Array(8000),rate);assert.equal(silence.bpm,null);assert.equal(silence.rms,0);
});
test('regeneration preserves locked object identity and avoids duplicate sample choices',()=>{
 const pantry=roles.flatMap((role,i)=>[0,1].map(v=>({id:`${i}-${v}`,role})));const layers=roles.map((_,i)=>({sample:pantry[2*i],locked:i===0,gain:.7}));const next=regenerate(layers,pantry,()=>.99);assert.equal(next[0],layers[0]);for(let i=1;i<5;i++)assert.notEqual(next[i].sample.id,layers[i].sample.id);assert.equal(new Set(next.map(l=>l.sample.id)).size,5);
});
test('compatibility is calculated and penalizes tempo or role mismatches',()=>{
 const good=roles.map(role=>({gain:1,sample:{bpm:120,rms:.1,role}}));const bad=roles.map(()=>({gain:1,sample:{bpm:60,rms:.1,role:'drums'}}));assert.equal(compatibility([],120).score,null);assert.ok(compatibility(good,120).score>compatibility(bad,120).score);
});
test('PCM WAV header, stereo interleave and clipping are valid',()=>{
 const wav=encodeWav([Float32Array.of(-2,.5),Float32Array.of(2,-.5)],44100),v=new DataView(wav);assert.equal(wav.byteLength,52);assert.equal(v.getUint32(24,true),44100);assert.equal(v.getUint16(22,true),2);assert.equal(v.getInt16(44,true),-32768);assert.equal(v.getInt16(46,true),32767);assert.equal(v.getInt16(48,true),16384);
});
test('stem archive has correct ZIP records, data size, CRC and entry count',async()=>{
 const blob=zipStore([{name:'1-drums.wav',data:Uint8Array.of(1,2,3).buffer},{name:'2-bass.wav',data:new ArrayBuffer(0)}]);const bytes=await blob.arrayBuffer(),v=new DataView(bytes);assert.equal(v.getUint32(0,true),0x04034b50);assert.equal(v.getUint32(14,true),0x55bc801d);assert.equal(v.getUint32(18,true),3);assert.equal(v.getUint16(bytes.byteLength-12,true),2);assert.equal(v.getUint32(bytes.byteLength-22,true),0x06054b50);
});
