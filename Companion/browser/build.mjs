import { readdir, cp, rm } from 'node:fs/promises';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
const root = fileURLToPath(new URL('../',import.meta.url));
for (const dir of ['browser','browser/lib','browser/public','browser/test']) {
  for(const name of await readdir(root+dir)) if(name.endsWith('.mjs')) {
    const result=spawnSync(process.execPath,['--check',root+dir+'/'+name],{stdio:'inherit'});
    if(result.status!==0)process.exit(result.status||1);
  }
}
await rm(root+'dist',{recursive:true,force:true});
await cp(root+'browser/public',root+'dist',{recursive:true});
console.log('Production browser files created in dist/. Run npm run preview for the local Splice companion.');
