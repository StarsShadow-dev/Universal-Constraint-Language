import * as fs from 'fs/promises';

import { lex } from './lexer';
import { parse } from './parser';
import { build } from './builder';

export async function compileFile(filePath: string) {
	const text = await fs.readFile(filePath, { encoding: 'utf8' });
	
	console.log("text:", text);
	
	let tokens = lex(text);
	console.log("tokens:", tokens);
	
	let AST = parse(tokens, 0);
	console.log("AST:", AST);
	
	let scopeList = build(AST);
	console.log("scopeList:", scopeList);
}

export function test() {
	return "from test";
}