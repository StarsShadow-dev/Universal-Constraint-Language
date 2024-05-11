import { ASTnode, CodeGenText, ScopeObject, Token, getCGText, unwrapScopeObject } from "./types";
import { lex } from "./lexer";
import {
	ParserMode,
	parse,
} from './parser';
import { build, callFunction } from "./builder";
import utilities from "./utilities";
import codeGen from "./codeGen";
import logger from "./logger";

export type IdeOptions = {
	mode: "compileFile",
}

export type CompilerOptions = {
	filePath: string,
	check: boolean,
	
	outputPath?: string,
	ideOptions?: IdeOptions,
}

export type ScopeInformation = {
	levels: ScopeObject[][];
	currentLevel: number;

	function: (ScopeObject & { kind: "function"; }) | null;
	functionArgumentNameText: string,

	// the function that is being generated
	generatingFunction: (ScopeObject & { kind: "function"; }) | null;
};

export type BuilderOptions = {
	compileTime: boolean;
	codeGenText: CodeGenText;
	disableValueEvaluation: boolean;
	// getStruct: boolean,
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
	
	inCheckMode: boolean,
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
		
		inCheckMode: false,
	};
}

// filePath -> FileContext
let compiledFiles: any = {};

export function resetCompiledFiles() {
	compiledFiles = {};
}

function checkScopeLevel(context: BuilderContext, level: ScopeObject[]) {
	for (let i = 0; i < level.length; i++) {
		const alias = level[i];
		if (alias.kind == "alias" && alias.isAfield) {
			continue;
		}
		const value = unwrapScopeObject(alias);
		if (value.kind == "function") {
			callFunction(context, value, null, "builtin", false, null, null, null, true);
		} else if (value.kind == "typeUse" && value.type.kind == "struct") {
			checkScopeLevel(context, value.type.members);
		}
	}
}

export function compile(options: CompilerOptions, onTokens: null | ((tokens: Token[]) => void)): BuilderContext {
	const context = newBuilderContext();
	
	context.file = compileFile(context, options.filePath, onTokens);
	
	const exportStart = Date.now();
	
	codeGen.start(context);
	
	if (options.check) {
		context.inCheckMode = true;
		checkScopeLevel(context, context.file.scope.levels[0]);
		context.inCheckMode = false;
	}
	
	for (let i = 0; i < context.exports.length; i++) {
		const toExport = context.exports[i];
		if (toExport.kind == "function") {
			callFunction(context, toExport, null, "builtin", false, null, null, null);
		}
	}
	
	logger.addTime("exporting", Date.now() - exportStart);
	
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
			functionArgumentNameText: "",
			generatingFunction: null,
		},
	};
	context.file = newFile;
	compiledFiles[filePath] = newFile;
	
	// get tokens
	const text = utilities.readFile(filePath);
	// console.log("text:", text);
	
	const lexStart = Date.now();
	const tokens = lex(filePath, text);
	logger.addTime("lexing", Date.now() - lexStart);
	
	// console.log("tokens:", tokens);
	
	if (onTokens) {
		onTokens(tokens);
	}
	
	// get AST
	const parseStart = Date.now();
	let AST: ASTnode[] = [];
	AST = parse({
		tokens: tokens,
		i: 0,
	}, ParserMode.normal, null);
	logger.addTime("parsing", Date.now() - parseStart);
	
	// console.log(`AST '${filePath}':`, JSON.stringify(AST, undefined, 4));
	
	// build
	const buildStart = Date.now();
	const scopeList = build(context, AST, null, null, false, false, false);
	logger.addTime("building", Date.now() - buildStart);
	
	// console.log(`top '${filePath}':\n\n${codeGen.getTop().join("")}`);
	// console.log("scopeList:", JSON.stringify(scopeList, undefined, 4));
	
	context.file = oldFile;
	return newFile;
}