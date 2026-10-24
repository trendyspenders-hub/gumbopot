import { SPLICE_URL, spliceFetch } from './lib/splice.mjs';
const response = await spliceFetch(SPLICE_URL);
const meta = await (await spliceFetch('https://mcp.splice.com/.well-known/oauth-protected-resource')).json();
const oauth = await (await spliceFetch('https://mcp.splice.com/.well-known/oauth-authorization-server')).json();
if (response.status !== 401 || meta.resource !== SPLICE_URL || oauth.issuer !== 'https://mcp.splice.com' || !oauth.code_challenge_methods_supported?.includes('S256')) throw new Error('Splice discovery did not match the expected service.');
console.log(JSON.stringify({ endpoint: SPLICE_URL, reachable: true, authenticationRequired: true, pkce: 'S256', registrationPublished: !!oauth.registration_endpoint, connected: false, note: 'No sign-in, account data, or downloads performed.' },null,2));
