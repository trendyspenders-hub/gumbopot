/** Optional real-browser smoke test. Install Playwright separately; see VERIFICATION.md. */
import {createRequire} from 'node:module';
import {mkdtemp,writeFile,readFile,rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import assert from 'node:assert/strict';
import {startServer} from './server.mjs';
import {encodeWav} from './public/core.mjs';
const require=createRequire(import.meta.url);
let chromium;
try{({chromium}=require(process.env.GUMBO_PLAYWRIGHT_PATH||'playwright'));}catch{throw new Error('Install Playwright for this optional check: npm install --no-save --package-lock=false playwright, then npx playwright install chromium.');}
const app=await startServer({port:0}),work=await mkdtemp(join(tmpdir(),'gumbo-browser-'));
let browser;
try{
 browser=await chromium.launch({headless:true,executablePath:process.env.GUMBO_CHROMIUM_EXECUTABLE||undefined,args:['--no-sandbox','--autoplay-policy=no-user-gesture-required']});
 const page=await browser.newPage({viewport:{width:1536,height:1100},acceptDownloads:true}),errors=[];
 page.on('pageerror',e=>errors.push(e.message));await page.goto(app.origin);
 await page.locator('#demo').click();await page.waitForFunction(()=>document.getElementById('count').textContent==='10');
 await page.locator('#play').click();await page.waitForFunction(()=>document.getElementById('play-state').textContent.startsWith('Simmering'));
 const first=await page.locator('.bowl .sample-name').first().textContent();await page.getByRole('button',{name:'□ Lock bowl',exact:true}).first().click();await page.locator('#stir').click();assert.equal(await page.locator('.bowl .sample-name').first().textContent(),first);await page.locator('#stop').click();
 const wav=page.waitForEvent('download');await page.locator('#mix').click();const mixPath=join(work,'mix.wav');await(await wav).saveAs(mixPath);const bytes=await readFile(mixPath);assert.equal(bytes.toString('ascii',0,4),'RIFF');assert.equal(bytes.readUInt16LE(22),2);assert.equal(bytes.readUInt32LE(24),44100);assert.equal(bytes.readUInt32LE(40),352800*4);assert.ok(bytes.subarray(44).some(x=>x));
 const zip=page.waitForEvent('download');await page.locator('#stems').click();const zipPath=join(work,'stems.zip');await(await zip).saveAs(zipPath);const stems=await readFile(zipPath);assert.equal(stems.readUInt16LE(stems.length-12),5);
 const input=join(work,'test_drums_120bpm.wav'),data=Float32Array.from({length:44100},(_,i)=>.15*Math.sin(2*Math.PI*220*i/44100));await writeFile(input,Buffer.from(encodeWav([data],44100)));await page.locator('#files').setInputFiles(input);await page.waitForFunction(()=>document.getElementById('count').textContent==='11');
 page.once('dialog',async d=>d.accept('Smoke recipe'));await page.locator('#save').click();await page.waitForFunction(()=>document.querySelectorAll('#recipes button').length===1);
 await page.setViewportSize({width:390,height:844});assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);assert.deepEqual(errors,[]);
 console.log('PASS: real browser demo playback, locked regeneration, audio import, non-silent stereo WAV, five-stem ZIP, recipe save, mobile layout and no JavaScript errors.');
}finally{await browser?.close();await app.close();await rm(work,{recursive:true,force:true});}
