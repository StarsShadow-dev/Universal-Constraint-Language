import * as fs from 'fs/promises';

import { lex } from './lexer';
import { parse } from './parser';

export async function compileFile(filePath: string) {
	const text = await fs.readFile(filePath, { encoding: 'utf8' });
	
	console.log("text:", text);
	
	let tokens = lex(text);
	console.log("tokens:", tokens);
	
	let AST = parse(tokens, 0);
	console.log("AST:", AST);
}

export function test() {
	return "from test";
}