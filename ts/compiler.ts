import { ASTnode, ScopeObject, getCGText } from "./types";
import { lex } from "./lexer";
import {
	ParserMode,
	parse,
} from './parser';
import { BuilderContext, build } from "./builder";
import utilities from "./utilities";
import codeGen from "./codeGen";

// filePath -> context
const compiledFiles: any = {};

export function compileFile(filePath: string): ScopeObject[][] {
	if (compiledFiles[filePath]) {
		return compiledFiles[filePath].scope.levels;
	}
	
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
		topCodeGenText: getCGText(),
		
		filePath: filePath,
		
		scope: {
			levels: [],
			currentLevel: -1,
			function: null,
			generatingFunction: null,
		},
		
		options: {
			compileTime: false,
			codeGenText: [],
			disableValueEvaluation: false,
		}
	};
	compiledFiles[filePath] = builderContext;
	
	codeGen.start(builderContext);
	const scopeList = build(builderContext, AST, null, null, false);
	// console.log(`top '${filePath}':\n\n${codeGen.getTop().join("")}`);
	
	// console.log("scopeList:", JSON.stringify(scopeList, undefined, 4));
	
	return builderContext.scope.levels;
}