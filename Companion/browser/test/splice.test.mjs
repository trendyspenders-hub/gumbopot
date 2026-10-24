import test from 'node:test';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {MemoryOAuthProvider,ConsentGate,SpliceAdapter,toolNeedsConsent,sameSecret,spliceFetch} from '../lib/splice.mjs';
const issuer='https://mcp.splice.com';
test('OAuth state rejects forged, expired and replayed callbacks',()=>{
 let now=0;const p=new MemoryOAuthProvider('http://127.0.0.1:5173/oauth/callback',{now:()=>now});p.begin();const state=p.state();assert.throws(()=>p.consumeState('wrong'));p.consumeState(state);assert.throws(()=>p.consumeState(state));p.begin();const old=p.state();now=600001;assert.throws(()=>p.consumeState(old));
});
test('credentials are issuer-bound and cleared without storing on disk',()=>{
 const p=new MemoryOAuthProvider('http://127.0.0.1:5173/oauth/callback');const ctx={issuer};p.saveClientInformation({client_id:'test',issuer},ctx);p.saveTokens({access_token:'test',token_type:'Bearer',issuer},ctx);assert.equal(p.tokens().access_token,'test');assert.equal(p.tokens({issuer:'https://other.invalid'}),undefined);assert.throws(()=>p.saveTokens({access_token:'secret'},{issuer:'https://other.invalid'}));p.clear();assert.equal(p.tokens(),undefined);assert.equal(p.clientInformation(ctx),undefined);
});
test('approval binds exact arguments, expires, and is single-use',()=>{
 let now=0;const gate=new ConsentGate({now:()=>now});const ticket=gate.issue('download',{id:1});assert.throws(()=>gate.consume(ticket,'download',{id:2}));assert.throws(()=>gate.consume(ticket,'download',{id:1}));const good=gate.issue('download',{id:1});gate.consume(good,'download',{id:1});assert.throws(()=>gate.consume(good,'download',{id:1}));const expired=gate.issue('search',{});now=120001;assert.throws(()=>gate.consume(expired,'search',{}));
});
test('credit and mutation actions require consent even with misleading hints',()=>{
 assert.equal(toolNeedsConsent({name:'download_sample',annotations:{readOnlyHint:true}}),true);assert.equal(toolNeedsConsent({name:'anything'}),true);assert.equal(toolNeedsConsent({name:'search',annotations:{readOnlyHint:true}}),false);assert.equal(sameSecret('a','aa'),false);
});
test('network adapter rejects untrusted origins before making a request',()=>{assert.throws(()=>spliceFetch('http://127.0.0.1/private'));assert.throws(()=>spliceFetch('https://attacker.invalid/oauth/token'));});
test('actual SDK completes mock OAuth PKCE, discovers tools, and executes only approved calls',async()=>{
 let called=0,challenge,registration,tokenRequest;
 const json=(obj,status=200)=>new Response(JSON.stringify(obj),{status,headers:{'Content-Type':'application/json'}});
 const mockFetch=async(input,init={})=>{
  const url=new URL(typeof input==='string'||input instanceof URL?input:input.url);const headers=new Headers(init.headers);
  if(url.pathname.includes('oauth-protected-resource'))return json({resource:issuer+'/mcp',authorization_servers:[issuer],scopes_supported:['openid']});
  if(url.pathname.includes('oauth-authorization-server')||url.pathname.includes('openid-configuration'))return json({issuer,authorization_endpoint:issuer+'/oauth/authorize',token_endpoint:issuer+'/oauth/token',registration_endpoint:issuer+'/oauth/register',response_types_supported:['code'],grant_types_supported:['authorization_code','refresh_token'],token_endpoint_auth_methods_supported:['none'],code_challenge_methods_supported:['S256']});
  if(url.pathname==='/oauth/register'){registration=JSON.parse(init.body);return json({...registration,client_id:'test-client'},201);}
  if(url.pathname==='/oauth/token'){tokenRequest=new URLSearchParams(init.body);return json({access_token:'fake-only-for-test',token_type:'Bearer',expires_in:3600});}
  if(url.pathname==='/mcp'){
   if(!headers.get('Authorization'))return new Response('no bearer token',{status:401,headers:{'WWW-Authenticate':`Bearer resource_metadata="${issuer}/.well-known/oauth-protected-resource"`}});
   if((init.method||'GET')==='GET')return new Response('',{status:405});
   const message=JSON.parse(init.body);
   if(message.id === undefined)return new Response(null,{status:202});
   const result=message.method==='initialize'?{protocolVersion:message.params.protocolVersion,capabilities:{tools:{}},serverInfo:{name:'mock-splice',version:'test'}}:message.method==='tools/list'?{tools:[{name:'search_samples',description:'Mock only',inputSchema:{type:'object',properties:{query:{type:'string'}},required:['query']},annotations:{readOnlyHint:true}},{name:'download_sample',inputSchema:{type:'object'}}]}:message.method==='tools/call'?(called++,{content:[{type:'text',text:'mock result'}]}):{};
   return json({jsonrpc:'2.0',id:message.id,result});
  }throw new Error('Unexpected request: '+url);
 };
 const adapter=new SpliceAdapter('http://127.0.0.1:5173/oauth/callback',{fetchFn:mockFetch});
 try{
  const start=await adapter.begin();assert.equal(start.phase,'awaiting_sign_in');assert.equal(start.connected,false);
  const auth=new URL(start.authorizationUrl);assert.equal(auth.origin,issuer);assert.equal(auth.searchParams.get('code_challenge_method'),'S256');challenge=auth.searchParams.get('code_challenge');assert.deepEqual(registration.redirect_uris,['http://127.0.0.1:5173/oauth/callback']);
  await assert.rejects(()=>adapter.finish(new URLSearchParams({state:'forged',code:'code'})));assert.equal(adapter.phase,'awaiting_sign_in');
  await adapter.finish(new URLSearchParams({state:auth.searchParams.get('state'),code:'mock-code',iss:issuer}));
  assert.equal(adapter.status().connected,true);assert.equal(adapter.tools.length,2);
  assert.equal(createHash('sha256').update(tokenRequest.get('code_verifier')).digest('base64url'),challenge);
  const action=adapter.review('download_sample',{id:1});assert.equal(action.mayChangeLibraryOrSpendCredits,true);assert.equal(called,0);
  await adapter.execute(action.name,action.args,action.ticket);assert.equal(called,1);
  await assert.rejects(()=>adapter.execute(action.name,action.args,action.ticket));assert.equal(called,1);
 }finally{await adapter.disconnect();}
 assert.equal(adapter.status().connected,false);assert.equal(adapter.provider.tokens(),undefined);
});
