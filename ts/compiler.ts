import * as fs from 'fs/promises';

import { Token } from './types';
import { lex } from './lexer';

export async function compileFile(filePath: string) {
	const text = await fs.readFile(filePath, { encoding: 'utf8' });
	
	console.log("text:", text);
	
	let tokens: Token[] = lex(text);
	
	console.log("tokens:", tokens);
}

export function test() {
	return "from test";
}