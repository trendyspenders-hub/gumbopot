import test from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';
import {startServer} from '../server.mjs';
const fake=()=>({status:()=>({phase:'disconnected',connected:false}),tools:[],begin:async()=>({connected:false}),disconnect:async()=>{},finish:async()=>{throw new Error('invalid state');},review:()=>({ticket:'test'}),execute:async()=>({content:[]})});
test('local companion rejects foreign origins, spoofed hosts, missing CSRF, invalid JSON and traversal',async()=>{
 const app=await startServer({port:0,adapterFactory:fake});try{
  const get=await fetch(app.origin+'/api/status'),s=await get.json();assert.equal(s.connected,false);assert.ok(s.csrf);
  assert.equal((await fetch(app.origin+'/api/status',{headers:{Origin:'https://evil.invalid'}})).status,403);
  const spoofed = await new Promise((resolve,reject)=>{const req=http.get(app.origin+'/api/status',{headers:{Host:'evil.invalid'}},res=>{res.resume();resolve(res.statusCode);});req.on('error',reject);});
  assert.equal(spoofed,403);
  assert.equal((await fetch(app.origin+'/api/splice/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:'{}'})).status,403);
  assert.equal((await fetch(app.origin+'/api/splice/connect',{method:'POST',headers:{'Content-Type':'application/json','X-Gumbo-CSRF':s.csrf},body:'{'})).status,400);
  assert.equal((await fetch(app.origin+'/api/splice/connect',{method:'POST',headers:{'Content-Type':'application/json','X-Gumbo-CSRF':s.csrf},body:'{}'})).status,200);
  assert.equal((await fetch(app.origin+'/Source/PluginProcessor.cpp')).status,404);
  const callback=await fetch(app.origin+'/oauth/callback?state=bad&code=bad',{redirect:'manual'});assert.equal(callback.status,303);assert.equal(callback.headers.get('Location'),'/?splice=failed');
  const home=await fetch(app.origin);assert.equal(home.status,200);assert.match(home.headers.get('Content-Security-Policy'),/frame-ancestors 'none'/);
 }finally{await app.close();}
});
