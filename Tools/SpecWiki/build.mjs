/*
 File   : build.mjs
 Desc   : 正本Markdownを単一のオフライン仕様書HTMLへ変換する
 Date   : 2026/11/02
 Author : Yokoyama Haruki
*/
import { readFile, writeFile, readdir, access } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Marked, Renderer } from './vendor/marked.mjs';

const toolDirectory = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(toolDirectory, '../..');
const docsDirectory = path.join(root, 'Docs');
const outputFile = path.join(docsDirectory, 'ImplementationWiki.html');
const readText = async file => (await readFile(file, 'utf8')).replace(/^\uFEFF/, '');
const catalog = JSON.parse(await readText(path.join(toolDirectory, 'catalog.json')));
const escapeHtml = text => String(text).replace(/[&<>"']/g, value => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[value]);
const slash = value => value.split(path.sep).join('/');
const route = values => `#${new URLSearchParams(values)}`;
const slug = value => value.toLowerCase().trim().replace(/<[^>]*>/g, '').replace(/[^\p{L}\p{N}_\s-]/gu, '').replace(/\s+/g, '-') || 'section';

async function FindMarkdown(directory)
{
    const files = [];
    for (const entry of await readdir(directory, { withFileTypes: true }))
    {
        const file = path.join(directory, entry.name);
        if (entry.isDirectory()) files.push(...await FindMarkdown(file));
        else if (entry.name.endsWith('.md')) files.push(file);
    }
    return files.sort();
}

const categories = new Map(catalog.categories.map(category => [category.id, category]));
if (categories.size !== catalog.categories.length) throw new Error('カテゴリIDが重複しています');
for (const category of catalog.categories)
{
    const ancestors = new Set([category.id]);
    let parent = category.parent;
    while (parent)
    {
        if (!categories.has(parent) || ancestors.has(parent)) throw new Error(`カテゴリ階層が不正: ${category.id}`);
        ancestors.add(parent);
        parent = categories.get(parent).parent;
    }
}

const pages = [];
const markdown = new Marked({ gfm: true, breaks: false });
for (const file of await FindMarkdown(docsDirectory))
{
    const source = slash(path.relative(docsDirectory, file));
    const settings = catalog.pages[source] ?? {};
    const group = catalog.groups.find(group => source.startsWith(`${group.directory}/`));
    const category = settings.category ?? group?.category;
    if (!categories.has(category)) throw new Error(`未分類のMarkdownです。catalog.jsonへ分類を追加してください: ${source}`);
    const content = await readText(file);
    const tokens = markdown.lexer(content);
    const titleToken = tokens.find(token => token.type === 'heading' && token.depth === 1);
    if (!titleToken) throw new Error(`タイトル見出しがありません: ${source}`);
    const title = titleToken.text.replace(/`/g, '');
    const firstParagraph = tokens.find(token => token.type === 'paragraph' && !/^\[|^根拠[:：]|^関連設計[:：]/.test(token.text));
    const summary = settings.summary ?? firstParagraph?.text.replace(/\[([^\]]+)\]\([^)]+\)/g, '$1').replace(/[`*_]/g, '') ?? title;
    const headings = [];
    const counts = new Map();
    for (const token of tokens.filter(token => token.type === 'heading'))
    {
        const text = token.text.replace(/[`*_]/g, '');
        const base = slug(text);
        const count = counts.get(base) ?? 0;
        counts.set(base, count + 1);
        const id = count ? `${base}-${count}` : base;
        token.wikiId = id;
        if (token !== titleToken) headings.push({ id, text, depth: token.depth });
    }
    pages.push({ source, category, title, summary, label: settings.label ?? (path.basename(file) === 'Types.md' ? '型の解説' : path.basename(file) === 'README.md' && source !== 'Implementation/README.md' ? '処理・構造' : title), order: settings.order ?? (path.basename(file) === 'README.md' ? 10 : 20), headings, diagrams: settings.diagrams ?? [], notes: settings.notes ?? [], tokens, titleToken });
}

const pagesBySource = new Map(pages.map(page => [page.source, page]));
const headingIds = new Map(pages.map(page => [page.source, page.tokens.filter(token => token.type === 'heading').map(token => token.wikiId)]));
const sourceRoutes = {};
for (const page of pages)
{
    const group = catalog.groups.find(group => page.source.startsWith(`${group.directory}/`));
    const primary = group && pages.find(candidate => candidate.source === `${group.directory}/README.md`);
    const section = primary ? `document-${slug(path.posix.relative(group.directory, page.source).replace(/\.md$/, ''))}` : '';
    sourceRoutes[page.source] = { page: primary?.source ?? page.source, ...(section ? { section } : {}) };
}
for (const source of Object.keys(catalog.pages))
    if (!pagesBySource.has(source)) throw new Error(`分類設定の文書が見つかりません: ${source}`);

const warnings = new Set();
function ResolveLink(href, source)
{
    if (/^https?:\/\//i.test(href)) return { href, kind: 'external', title: '外部サイト（通信が必要）' };
    if (/^mailto:/i.test(href)) return { href, kind: 'external', title: 'メールリンク' };
    if (/^[a-z][a-z\d+.-]*:/i.test(href)) throw new Error(`未対応のリンク形式: ${source}: ${href}`);
    const [pathname, fragment] = href.split('#', 2);
    const target = path.resolve(docsDirectory, path.dirname(source), decodeURIComponent(pathname));
    const document = pagesBySource.get(slash(path.relative(docsDirectory, pathname ? target : path.join(docsDirectory, source))));
    if (document)
    {
        const anchor = fragment ? decodeURIComponent(fragment) : '';
        const ids = headingIds.get(document.source);
        if (anchor && !ids.includes(anchor)) throw new Error(`見出しリンクが見つかりません: ${source}: ${href}`);
        const destination = sourceRoutes[document.source];
        const section = destination.section ? destination.section + (anchor ? `-${anchor}` : '') : anchor;
        return { href: route({ page: destination.page, ...(section ? { section } : {}) }) };
    }
    return { href: slash(path.relative(docsDirectory, target)).split('/').map(encodeURIComponent).join('/') + (fragment ? `#${fragment}` : ''), target, kind: 'local', title: 'リポジトリ内のファイル（HTML単体のコピー先では参照できません）' };
}

for (const page of pages)
{
    const renderer = new Renderer();
    renderer.heading = function(token)
    {
        if (token === page.titleToken) return '';
        const destination = sourceRoutes[page.source];
        const depth = Math.min(6, token.depth + (destination.section ? 1 : 0));
        const section = destination.section ? `${destination.section}-${token.wikiId}` : token.wikiId;
        return `<h${depth} id="section-${escapeHtml(section)}">${this.parser.parseInline(token.tokens)}<a class="heading-link" aria-label="この見出しへのリンク" href="${escapeHtml(route({ page: destination.page, section }))}">#</a></h${depth}>\n`;
    };
    renderer.link = function(token)
    {
        const resolved = ResolveLink(token.href, page.source);
        const text = this.parser.parseInline(token.tokens);
        return `<a href="${escapeHtml(resolved.href)}"${resolved.kind ? ` class="${resolved.kind}-link" title="${escapeHtml(resolved.title)}"` : ''}>${text}</a>`;
    };
    renderer.html = token => escapeHtml(token.text);
    renderer.image = () => { throw new Error(`画像の埋め込み対応が必要です: ${page.source}`); };
    renderer.table = function(token) { return `<div class="table-scroll" role="region" tabindex="0" aria-label="表（横スクロール可能）">${Renderer.prototype.table.call(this, token)}</div>`; };
    page.html = new Marked({ gfm: true, breaks: false, renderer }).parser(page.tokens);
    for (const token of markdown.walkTokens(page.tokens, token => token))
    {
        if (token.type !== 'link') continue;
        const resolved = ResolveLink(token.href, page.source);
        if (resolved.target)
        {
            const exists = await access(resolved.target).then(() => true, () => false);
            if (!exists) warnings.add(`${page.source}: ${token.href}`);
        }
    }
    delete page.tokens;
    delete page.titleToken;
}

const diagrams = {};
for (const [id, diagram] of Object.entries(catalog.diagrams))
{
    const svg = await readText(path.join(toolDirectory, 'diagrams', `${id}.svg`));
    if (!svg.includes('<svg') || !svg.includes('viewBox=') || /<(script|foreignObject)\b|\bon\w+\s*=|(?:href|src)\s*=\s*["'](?:https?:|\/\/)/i.test(svg)) throw new Error(`SVGを確認してください: ${id}`);
    for (const source of diagram.sources) await access(path.join(root, source));
    diagrams[id] = { ...diagram, svg };
}
for (const page of pages)
    for (const id of page.diagrams)
        if (!diagrams[id]) throw new Error(`未定義の図解: ${id}`);

const articles = [];
for (const page of pages)
{
    if (sourceRoutes[page.source].page !== page.source) continue;
    const documents = pages.filter(document => sourceRoutes[document.source].page === page.source).sort((a, b) => a.order - b.order || a.title.localeCompare(b.title, 'ja'));
    if (!sourceRoutes[page.source].section)
    {
        articles.push({ ...page, sources: [page.source], sections: [] });
        continue;
    }

    const sections = documents.map(document => ({ id: sourceRoutes[document.source].section, source: document.source, title: document.title, label: document.label }));
    if (new Set(sections.map(section => section.id)).size !== sections.length) throw new Error(`記事内の文書IDが重複: ${page.source}`);
    const html = documents.map((document, index) => {
        const section = sections[index];
        const href = escapeHtml(route({ page: page.source, section: section.id }));
        return `<section class="document-section" id="section-${escapeHtml(section.id)}"><h2>${escapeHtml(document.title)}<a class="heading-link" aria-label="この見出しへのリンク" href="${href}">#</a></h2><p class="document-source"><a class="local-link" href="${document.source.split('/').map(encodeURIComponent).join('/')}">正本Markdown</a>：${escapeHtml(document.source)}</p>${document.html}</section>`;
    }).join('\n');
    articles.push({ ...page, title: categories.get(page.category).title, label: categories.get(page.category).title, summary: categories.get(page.category).description, html, sources: documents.map(document => document.source), sections, headings: documents.flatMap(document => [{ id: sourceRoutes[document.source].section, text: document.label, depth: 2 }, ...document.headings.map(heading => ({ ...heading, id: `${sourceRoutes[document.source].section}-${heading.id}`, depth: Math.min(6, heading.depth + 1) }))]), notes: documents.flatMap(document => document.notes), diagrams: [...new Set(documents.flatMap(document => document.diagrams))] });
}

const payload = { categories: catalog.categories, pages: articles, sourceRoutes, diagrams, warnings: [...warnings], renderer: 'marked 17.0.5 / MIT' };
const serialized = JSON.stringify(payload).replace(/</g, '\\u003c').replace(/\u2028/g, '\\u2028').replace(/\u2029/g, '\\u2029');
const template = await readText(path.join(toolDirectory, 'template.html'));
if (template.split('<!-- WIKI_DATA -->').length !== 2) throw new Error('テンプレートのデータ挿入位置が不正です');
const html = template.replace('<!-- WIKI_DATA -->', `<script type="application/json" id="wiki-data">${serialized}</script>`);
if (process.argv.includes('--check'))
{
    if (await readText(outputFile) !== html) throw new Error('生成HTMLが古くなっています。Generate.cmdを実行してください');
    console.log(`PASS: ${articles.length}記事（正本${pages.length}文書） / ${Object.keys(diagrams).length}図 / 分類・リンク変換・生成内容一致`);
}
else
{
    await writeFile(outputFile, `\uFEFF${html}`, 'utf8');
    console.log(`生成: ${outputFile}\n${articles.length}記事（正本${pages.length}文書） / ${Object.keys(diagrams).length}図 / ${Buffer.byteLength(html).toLocaleString()} bytes`);
}
if (warnings.size) console.log(`既存Markdownの参照先不在（正本は未変更）:\n${[...warnings].map(item => `  ${item}`).join('\n')}`);
