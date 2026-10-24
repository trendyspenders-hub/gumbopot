import { randomBytes, timingSafeEqual, createHash } from 'node:crypto';
import { Client, StreamableHTTPClientTransport, UnauthorizedError } from '@modelcontextprotocol/client';
export const SPLICE_URL = 'https://mcp.splice.com/mcp';
const ISSUER = 'https://mcp.splice.com';
export const secret = () => randomBytes(32).toString('base64url');
export function sameSecret(a, b) {
  if (typeof a !== 'string' || typeof b !== 'string') return false;
  const x = Buffer.from(a), y = Buffer.from(b);
  return x.length === y.length && timingSafeEqual(x, y);
}
export class MemoryOAuthProvider {
  constructor(redirectUrl, { now = Date.now } = {}) {
    this.redirectUrl = redirectUrl; this.now = now;
    this.clients = new Map(); this.tokenSets = new Map();
    this.clientMetadata = { client_name: 'Gumbo Pot Local Companion', redirect_uris: [redirectUrl],
      application_type: 'native', grant_types: ['authorization_code','refresh_token'],
      response_types: ['code'], token_endpoint_auth_method: 'none' };
  }
  begin() { this.expectedState = secret(); this.expires = this.now() + 600000; this.authorizationUrl = undefined; }
  state() { if (!this.expectedState) throw new Error('Start a fresh sign-in.'); return this.expectedState; }
  consumeState(value) {
    const valid = this.now() < this.expires && sameSecret(value, this.expectedState);
    // Reject mismatches without consuming the legitimate in-flight login.
    if (!valid) throw new Error('Sign-in state is invalid or expired. Start again.');
    this.expectedState = undefined; this.expires = 0; this.authorizationUrl = undefined;
  }
  clientInformation(ctx) { return ctx ? this.clients.get(ctx.issuer) : undefined; }
  saveClientInformation(info, ctx) {
    const issuer = ctx?.issuer ?? info.issuer;
    if (issuer !== ISSUER) throw new Error('Unexpected Splice authorization issuer.');
    this.clients.set(issuer, info);
  }
  tokens(ctx) { return ctx ? this.tokenSets.get(ctx.issuer) : this.latestTokens; }
  saveTokens(tokens, ctx) {
    const issuer = ctx?.issuer ?? tokens.issuer;
    if (issuer !== ISSUER) throw new Error('Unexpected token issuer.');
    this.tokenSets.set(issuer, tokens); this.latestTokens = tokens;
  }
  redirectToAuthorization(url) {
    if (url.origin !== ISSUER || url.pathname !== '/oauth/authorize') throw new Error('Unexpected sign-in destination.');
    this.authorizationUrl = url.toString();
  }
  saveCodeVerifier(v) { this.verifier = v; }
  codeVerifier() { if (!this.verifier) throw new Error('Missing PKCE verifier. Start again.'); return this.verifier; }
  saveDiscoveryState(v) { this.discovery = v; }
  discoveryState() { return this.discovery; }
  invalidateCredentials(scope) {
    if (scope === 'all' || scope === 'client') this.clients.clear();
    if (scope === 'all' || scope === 'tokens') { this.tokenSets.clear(); this.latestTokens = undefined; }
    if (scope === 'all' || scope === 'verifier') this.verifier = undefined;
    if (scope === 'all' || scope === 'discovery') this.discovery = undefined;
  }
  clear() { this.invalidateCredentials('all'); this.expectedState = undefined; this.authorizationUrl = undefined; }
}
// Pin requests to the documented Splice origin. No server-provided URL can send tokens elsewhere.
export function spliceFetch(input, init = {}) {
  const url = new URL(typeof input === 'string' || input instanceof URL ? input : input.url);
  if (url.origin !== ISSUER) throw new Error('Splice requested an unexpected server origin.');
  return fetch(input, { ...init, redirect: 'error', signal: init.signal
    ? AbortSignal.any([init.signal, AbortSignal.timeout(25000)]) : AbortSignal.timeout(25000) });
}
export function toolNeedsConsent(tool) {
  return tool.annotations?.readOnlyHint !== true || tool.annotations?.destructiveHint === true ||
    /download|purchase|credit|save|create|edit|delete|remove|share|add|swap/i.test(tool.name);
}
export class ConsentGate {
  constructor({ now = Date.now } = {}) { this.now = now; this.pending = new Map(); }
  digest(name, args) { return createHash('sha256').update(JSON.stringify({ name, args })).digest('hex'); }
  issue(name, args) {
    for (const [key, value] of this.pending) if (value.until < this.now()) this.pending.delete(key);
    if (this.pending.size > 100) this.pending.clear();
    const ticket = secret(); this.pending.set(ticket, { hash: this.digest(name,args), until: this.now()+120000 }); return ticket;
  }
  consume(ticket, name, args) {
    const entry = this.pending.get(ticket); this.pending.delete(ticket);
    if (!entry || entry.until < this.now() || entry.hash !== this.digest(name,args)) throw new Error('Review this exact action again before running it.');
  }
  clear() { this.pending.clear(); }
}
export class SpliceAdapter {
  constructor(redirectUrl, { fetchFn = spliceFetch } = {}) {
    this.fetchFn = fetchFn; this.provider = new MemoryOAuthProvider(redirectUrl); this.gate = new ConsentGate();
    this.phase = 'disconnected'; this.tools = []; this.busy = false;
  }
  status() { return { phase: this.phase, connected: this.phase === 'connected', tools: this.tools.length }; }
  async exclusive(fn) {
    if (this.busy) throw new Error('A Splice operation is already in progress.');
    this.busy = true; try { return await fn(); } finally { this.busy = false; }
  }
  makeConnection() {
    this.client = new Client({ name: 'gumbo-pot-local-companion', version: '0.2.0' });
    this.transport = new StreamableHTTPClientTransport(new URL(SPLICE_URL), { authProvider: this.provider, fetch: this.fetchFn });
    this.client.onclose = () => { if (this.phase === 'connected') { this.phase = 'disconnected'; this.tools = []; } };
  }
  async begin() { return this.exclusive(async () => {
    if (this.phase === 'connected') return this.status();
    await this.transport?.close().catch(()=>{}); this.provider.begin(); this.makeConnection(); this.phase = 'connecting';
    try { await this.client.connect(this.transport); await this.discoverTools(); }
    catch (error) {
      if (error instanceof UnauthorizedError && this.provider.authorizationUrl) {
        this.phase = 'awaiting_sign_in'; return { ...this.status(), authorizationUrl: this.provider.authorizationUrl };
      }
      this.phase = 'disconnected'; throw error;
    }
    return this.status();
  }); }
  async finish(params) { return this.exclusive(async () => {
    if (this.phase !== 'awaiting_sign_in') throw new Error('No sign-in is waiting.');
    this.provider.consumeState(params.get('state'));
    if (params.has('error')) { this.phase = 'disconnected'; throw new Error('Splice sign-in was declined.'); }
    if (!params.get('code')) { this.phase = 'disconnected'; throw new Error('Splice did not supply an authorization code.'); }
    try {
      await this.transport.finishAuth(params);
      await this.transport.close(); this.makeConnection();
      await this.client.connect(this.transport); await this.discoverTools();
      this.provider.invalidateCredentials('verifier'); return this.status();
    } catch (error) { this.phase = 'disconnected'; throw error; }
  }); }
  async discoverTools() {
    const tools = []; let cursor; const seen = new Set();
    do {
      const page = await this.client.listTools(cursor ? {cursor} : {});
      tools.push(...page.tools); cursor = page.nextCursor;
      if (cursor && seen.has(cursor)) throw new Error('Splice returned a repeated tools cursor.');
      if (cursor) seen.add(cursor);
      if (tools.length > 500) throw new Error('Splice returned too many tools.');
    } while (cursor);
    this.tools = tools; this.phase = 'connected';
  }
  review(name, args) {
    if (this.phase !== 'connected') throw new Error('Connect to Splice first.');
    const tool = this.tools.find(t=>t.name===name); if (!tool) throw new Error('Unknown Splice tool. Reconnect to refresh.');
    if (!args || typeof args !== 'object' || Array.isArray(args)) throw new Error('Arguments must be a JSON object.');
    // Every operation is reviewed, including read-only tools; provider hints are never blanket permission.
    return { ticket: this.gate.issue(name,args), name, args, mayChangeLibraryOrSpendCredits: toolNeedsConsent(tool) };
  }
  async execute(name, args, ticket) { return this.exclusive(async () => {
    if (this.phase !== 'connected') throw new Error('Connect to Splice first.');
    this.gate.consume(ticket,name,args);
    try { return await this.client.callTool({ name, arguments: args }, undefined, { timeout: 60000 }); }
    catch(error) { if (error instanceof UnauthorizedError) { this.phase = 'disconnected'; this.tools = []; }
      throw error;
    }
  }); }
  async disconnect() { return this.exclusive(async () => {
    this.phase = 'disconnected'; this.gate.clear(); this.tools = [];
    await this.client?.close().catch(()=>{}); this.provider.clear();
  }); }
}
