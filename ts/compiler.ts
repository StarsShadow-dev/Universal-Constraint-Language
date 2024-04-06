import { ASTnode, ScopeObject } from "./types";
import { lex } from "./lexer";
import {
	ParserMode,
	parse,
} from './parser';
import { BuilderContext, build } from "./builder";
import utilities from "./utilities";

export function compileFile(filePath: string): ScopeObject[][] {
	const text = utilities.readFile(filePath);
	// console.log("text:", text);
	
	const tokens = lex(filePath, text);
	// console.log("tokens:", tokens);
	
	let AST: ASTnode[] = [];
	
	AST = parse({
		tokens: tokens,
		i: 0,
	}, ParserMode.normal, null);
	// console.log(`AST '${filePath}':`, JSON.stringify(AST, undefined, 4));
	
	const builderContext: BuilderContext = {
		filePath: filePath,
		
		scope: {
			levels: [],
			currentLevel: -1,
			function: null,
			generatingFunction: null,
		},
		
		options: {
			compileTime: false,
			codeGenText: [""],
		}
	};
	
	const scopeList = build(builderContext, AST, null, null);
	if (builderContext.options.codeGenText) {
		console.log("codeGenText:", builderContext.options.codeGenText[0]);
	}
	// console.log("scopeList:", JSON.stringify(scopeList, undefined, 4));
	
	return builderContext.scope.levels;
}