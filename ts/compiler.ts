import { ASTnode, CodeGenText, ScopeObject, Token, getCGText } from "./types";
import { lex } from "./lexer";
import {
	ParserMode,
	parse,
} from './parser';
import { build, callFunction } from "./builder";
import utilities from "./utilities";
import codeGen from "./codeGen";

export type ScopeInformation = {
	levels: ScopeObject[][];
	currentLevel: number;

	function: (ScopeObject & { kind: "function"; }) | null;

	// the function that is being generated
	generatingFunction: (ScopeObject & { kind: "function"; }) | null;
};

export type BuilderOptions = {
	compileTime: boolean;
	codeGenText: CodeGenText;
	disableValueEvaluation: boolean;
};

export type FileContext = {
	path: string;
	scope: ScopeInformation;
}

export type BuilderContext = {
	topCodeGenText: string[];
	options: BuilderOptions;
	nextSymbolName: number;
	exports: ScopeObject[];
	inIndentation: boolean,
	
	file: FileContext,
};

// file is null!
export function newBuilderContext(): BuilderContext {
	return {
		topCodeGenText: getCGText(),
		options: {
			compileTime: true,
			codeGenText: [],
			disableValueEvaluation: false,
		},
		nextSymbolName: 0,
		exports: [],
		inIndentation: true,
		
		file: null as any,
	};
}

// filePath -> FileContext
let compiledFiles: any = {};

export function resetCompiledFiles() {
	compiledFiles = {};
}

export function compile(startFilePath: string, onTokens: null | ((tokens: Token[]) => void)): BuilderContext {
	const context = newBuilderContext();
	
	context.file = compileFile(context, startFilePath, onTokens);
	
	codeGen.start(context);
	
	for (let i = 0; i < context.exports.length; i++) {
		const toExport = context.exports[i];
		if (toExport.kind == "function") {
			callFunction(context, toExport, null, "builtin", false, null, null, null);
		}
	}
	
	return context;
}

export function compileFile(context: BuilderContext, filePath: string, onTokens: null | ((tokens: Token[]) => void)): FileContext {
	// if the file has already been compiled, there is no point compiling it again
	// (this also lets two files import each other)
	if (compiledFiles[filePath]) {
		return compiledFiles[filePath];
	}
	
	const oldFile = context.file;
	const newFile = {
		path: filePath,
		scope: {
			levels: [],
			currentLevel: -1,
			function: null,
			generatingFunction: null,
		},
	};
	context.file = newFile;
	compiledFiles[filePath] = newFile;
	
	// get tokens
	const text = utilities.readFile(filePath);
	// console.log("text:", text);
	const tokens = lex(filePath, text);
	// console.log("tokens:", tokens);
	if (onTokens) {
		onTokens(tokens);
	}
	
	// get AST
	let AST: ASTnode[] = [];
	AST = parse({
		tokens: tokens,
		i: 0,
	}, ParserMode.normal, null);
	// console.log(`AST '${filePath}':`, JSON.stringify(AST, undefined, 4));
	
	// build
	const scopeList = build(context, AST, null, null, false, false, false);
	// console.log(`top '${filePath}':\n\n${codeGen.getTop().join("")}`);
	// console.log("scopeList:", JSON.stringify(scopeList, undefined, 4));
	
	context.file = oldFile;
	return newFile;
}