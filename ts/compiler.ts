import { ScopeObject } from "./types";
import { lex } from './lexer';
import {
	ParserMode,
	parse,
} from './parser';
import { BuilderContext, build } from './builder';
import utilities from './utilities';
import { CompileError } from "./report";

export function compileFile(filePath: string) {
	const text = utilities.readFile(filePath);
	
	// console.log("text:", text);
	
	const tokens = lex(filePath, text);
	// console.log("tokens:", tokens);
	
	const AST = parse({
		tokens: tokens,
		i: 0,
	}, ParserMode.normal, null);
	console.log("AST:", JSON.stringify(AST, undefined, 4));
	
	const builderContext: BuilderContext = {
		scopeLevels: [],
		level: -1,
	};
	
	let scopeList: ScopeObject[] = [];
	
	try {
		scopeList = build(builderContext, AST, null, null);
	} catch (error) {
		if (error instanceof CompileError) {
			console.log("uncaught compiler error");
			error.fatal();
		}
	}
	
	console.log("scopeList:", JSON.stringify(scopeList, undefined, 4));
	console.log("scopeLevels:", JSON.stringify(builderContext.scopeLevels, undefined, 4));
}

export function test() {
	return "from test";
}