import utilities from "./utilities";
import logger from "./logger";
import codeGen from "./codeGen";
import { lex } from "./lexer";
import { build, callFunction } from "./builder";
import { CompileError } from "./report";
import {
	ParserMode,
	parse,
} from './parser';
import {
	ASTnode,
	CodeGenText,
	ScopeObject,
	ScopeObject_alias,
	ScopeObject_function,
	Token,
	getCGText,
} from "./types";

export type IdeOptions = {
	mode: "compileFile",
}

export type CompilerOptions = {
	filePath: string,
	check: boolean,
	fancyErrors: boolean,
	
	outputPath?: string,
	ideOptions?: IdeOptions,
}

export type ScopeInformation = {
	levels: ScopeObject_alias[][];
	currentLevel: number;

	function: ScopeObject_function | null;
	functionArgumentNameText: string,

	// the function that is being generated
	generatingFunction: ScopeObject_function | null;
};

export type BuilderOptions = {
	compileTime: boolean;
	codeGenText: CodeGenText;
	disableDependencyAccess: boolean;
	// getStruct: boolean,
};

export type FileContext = {
	path: string;
	scope: ScopeInformation;
	AST: ASTnode[],
}

export type BuilderContext = {
	compilerOptions: CompilerOptions,
	topCodeGenText: string[];
	options: BuilderOptions;
	nextSymbolName: number;
	exports: ScopeObject[];
	inIndentation: boolean,
	file: FileContext,
	errors: CompileError[],
	
	inCheckMode: boolean,
};

// file is null!
export function newBuilderContext(compilerOptions: CompilerOptions): BuilderContext {
	return {
		compilerOptions: compilerOptions,
		topCodeGenText: getCGText(),
		options: {
			compileTime: true,
			codeGenText: [],
			disableDependencyAccess: false,
		},
		nextSymbolName: 0,
		exports: [],
		inIndentation: true,
		file: null as any,
		errors: [],
		
		inCheckMode: false,
	};
}

// filePath -> FileContext
let filesToCompile: any = {};

export function resetFilesToCompile() {
	filesToCompile = {};
}

function checkScopeLevel(context: BuilderContext, level: ScopeObject[]) {
	// utilities.TODO();
	// for (let i = 0; i < level.length; i++) {
	// 	const alias = level[i];
	// 	if (alias.kind == "alias" && alias.isAfield) {
	// 		continue;
	// 	}
	// 	const value = unwrapScopeObject(alias);
	// 	if (value.kind == "function" && !value.external && value.toBeChecked) {
	// 		callFunction(context, value, null, "builtin", false, null, null, null, true);
	// 	} else if (value.kind == "alias" && value.type && value.type. && value.toBeChecked) {
	// 		checkScopeLevel(context, value.type.members);
	// 	}
	// }
}

export function compile(context: BuilderContext, onTokens: null | ((tokens: Token[]) => void)) {
	let mainFile: FileContext;
	try {
		mainFile = compileFile(context, context.compilerOptions.filePath, onTokens);
	} catch (error) {
		if (error instanceof CompileError) {
			context.errors.push(error);
			return;
		} else if (error == "__done__") {
			return;
		} else {
			throw error;
		}
	}
	
	for (const key in filesToCompile) {
		context.file = filesToCompile[key];
		context.file.scope.levels[0] = [];
		const scopeList = build(context, filesToCompile[key].AST, {
			codeGenText: null,
			compileTime: true,
			disableDependencyAccess: false,
		}, null, false, false, true);
		context.file.scope.levels[0] = scopeList as ScopeObject_alias[];
	}
	
	context.file = mainFile;
	
	const checkStart = Date.now();
	if (context.compilerOptions.check) {
		context.inCheckMode = true;
		checkScopeLevel(context, context.file.scope.levels[0]);
		context.inCheckMode = false;
	}
	logger.addTime("checking", Date.now() - checkStart);
	
	const exportStart = Date.now();
	codeGen.start(context);
	for (let i = 0; i < context.exports.length; i++) {
		const toExport = context.exports[i];
		if (toExport.kind == "function") {
			callFunction(context, toExport, null, "builtin", false, null, null, null);
		}
	}
	logger.addTime("exporting", Date.now() - exportStart);
}

export function compileFile(context: BuilderContext, filePath: string, onTokens: null | ((tokens: Token[]) => void)): FileContext {
	// if the file has already been compiled, there is no point compiling it again
	// (this also lets two files import each other)
	if (filesToCompile[filePath]) {
		return filesToCompile[filePath];
	}
	
	const oldFile = context.file;
	const newFile: FileContext = {
		path: filePath,
		scope: {
			levels: [],
			currentLevel: -1,
			function: null,
			functionArgumentNameText: "",
			generatingFunction: null,
		},
		AST: [],
	};
	context.file = newFile;
	filesToCompile[filePath] = newFile;
	
	const text = utilities.readFile(filePath);
	// console.log("text:", text);
	
	const lexStart = Date.now();
	const tokens = lex(filePath, text);
	logger.addTime("lexing", Date.now() - lexStart);
	console.log("tokens:", tokens);
	
	if (onTokens) {
		onTokens(tokens);
	}
	
	const parseStart = Date.now();
	newFile.AST = parse({
		tokens: tokens,
		i: 0,
	}, ParserMode.normal, null);
	logger.addTime("parsing", Date.now() - parseStart);
	console.log(`AST '${filePath}':`, JSON.stringify(newFile.AST, undefined, 4));
	
	const scopeList = build(context, newFile.AST, {
		codeGenText: null,
		compileTime: true,
		disableDependencyAccess: true,
	}, null, false, false, true);
	context.file.scope.levels[0] = scopeList as ScopeObject_alias[];
	
	// console.log("scopeList:", JSON.stringify(scopeList, undefined, 4));
	
	context.file = oldFile;
	return newFile;
}