import * as fs from 'fs/promises';

import { lex } from './lexer';
import {
	ParserMode,
	parse,
} from './parser';
import { BuilderContext, build } from './builder';

export async function compileFile(filePath: string) {
	const text = await fs.readFile(filePath, { encoding: 'utf8' });
	
	console.log("text:", text);
	
	const tokens = lex(filePath, text);
	console.log("tokens:", tokens);
	
	const AST = parse({
		tokens: tokens,
		i: 0,
	}, ParserMode.normal);
	console.log("AST:", JSON.stringify(AST, undefined, 4));
	
	const builderContext: BuilderContext = {
		scopeLevels: [[]],
		level: -1,
	};
	const scopeList = build(builderContext, AST);
	console.log("scopeList:", JSON.stringify(scopeList, undefined, 4));
	console.log("scopeLevels:", JSON.stringify(builderContext.scopeLevels, undefined, 4));
}

export function test() {
	return "from test";
}