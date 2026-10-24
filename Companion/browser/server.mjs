import http from 'node:http';
import { readFile } from 'node:fs/promises';
import { dirname, resolve, extname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { SpliceAdapter, sameSecret, secret } from './lib/splice.mjs';
const here = dirname(fileURLToPath(import.meta.url));
const mime = { '.html':'text/html; charset=utf-8', '.css':'text/css; charset=utf-8', '.mjs':'text/javascript; charset=utf-8', '.svg':'image/svg+xml' };
export async function startServer({port = 5173, adapterFactory = url=>new SpliceAdapter(url), preview = false} = {}) {
  const csrf = secret(); let adapter, origin;
  const root = resolve(here, preview ? '../dist' : 'public');
  const server = http.createServer(async(req,res)=>{
    res.setHeader('Cache-Control','no-store'); res.setHeader('X-Content-Type-Options','nosniff');
    res.setHeader('Referrer-Policy','no-referrer'); res.setHeader('X-Frame-Options','DENY');
    res.setHeader('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; media-src 'self' blob:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'self'");
    const json = (status,obj)=>{res.writeHead(status,{'Content-Type':'application/json; charset=utf-8'});res.end(JSON.stringify(obj));};
    try {
      if (req.headers.host !== new URL(origin).host) return json(403,{error:'Open the exact local address printed in Terminal.'});
      if (req.headers.origin && req.headers.origin !== origin) return json(403,{error:'Cross-origin requests are not allowed.'});
      const url = new URL(req.url,origin);
      if (url.pathname === '/oauth/callback' && req.method === 'GET') {
        try { await adapter.finish(url.searchParams); res.writeHead(303,{Location:'/?splice=connected'}); res.end(); }
        catch { res.writeHead(303,{Location:'/?splice=failed'}); res.end(); }
        return;
      }
      if (url.pathname === '/api/status' && req.method === 'GET') return json(200,{...adapter.status(),csrf});
      if (url.pathname === '/api/splice/tools' && req.method === 'GET') return json(200,{tools:adapter.tools});
      if (url.pathname.startsWith('/api/')) {
        if (req.method !== 'POST') return json(405,{error:'POST required.'});
        if (!sameSecret(req.headers['x-gumbo-csrf'],csrf)) return json(403,{error:'Reload Gumbo Pot before trying again.'});
        if (!req.headers['content-type']?.startsWith('application/json')) return json(415,{error:'JSON required.'});
        let raw=''; for await (const chunk of req) { raw += chunk; if (Buffer.byteLength(raw)>65536) return json(413,{error:'Action exceeds 64 KB.'}); }
        let body; try { body=JSON.parse(raw || '{}'); } catch {return json(400,{error:'Invalid JSON.'});}
        switch (url.pathname) {
          case '/api/splice/connect': return json(200,await adapter.begin());
          case '/api/splice/disconnect': await adapter.disconnect(); return json(200,adapter.status());
          case '/api/splice/review': return json(200,adapter.review(body.name,body.args));
          case '/api/splice/execute': return json(200,{result:await adapter.execute(body.name,body.args,body.ticket)});
          default:return json(404,{error:'Unknown action.'});
        }
      }
      if(req.method !== 'GET' && req.method !== 'HEAD') return json(405,{error:'GET required.'});
      const name = url.pathname === '/' ? 'index.html' : url.pathname.slice(1);
      if (!['index.html','app.mjs','audio.mjs','core.mjs','style.css'].includes(name)) return json(404,{error:'Not found.'});
      const bytes = await readFile(resolve(root,name));
      res.writeHead(200,{'Content-Type':mime[extname(name)]||'application/octet-stream'}); res.end(req.method==='HEAD'?undefined:bytes);
    } catch(error) {
      // Never return SDK error bodies that could contain an OAuth code or token.
      json(502,{error: error.message?.startsWith('Connect to Splice') ? error.message : 'Splice could not complete this action. Check your connection, sign in again, or review the service requirements.'});
    }
  });
  await new Promise((ok,bad)=>{ server.once('error',bad);server.listen(port,'127.0.0.1',ok); });
  origin=`http://127.0.0.1:${server.address().port}`;adapter=adapterFactory(`${origin}/oauth/callback`);
  return {server,adapter,origin,close:async()=>{await adapter.disconnect();await new Promise((ok,bad)=>server.close(e=>e?bad(e):ok()));}};
}
if(process.argv[1] && resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  const port=Number(process.env.GUMBO_PORT||5173);
  if (!Number.isInteger(port)||port<1024||port>65535) throw new Error('GUMBO_PORT must be 1024–65535.');
  const app=await startServer({port,preview:process.argv.includes('--preview')});
  console.log(`Gumbo Pot: ${app.origin}\nSign in to Splice using the Connect button. Ctrl+C stops the app and clears local login tokens.`);
  for(const signal of ['SIGINT','SIGTERM'])process.on(signal,async()=>{await app.close();process.exit(0);});
}
