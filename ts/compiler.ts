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
	unwrapScopeObject,
} from "./types";

export enum CompilerStage {
	findAliases = 0,
	evaluateAliasDependencies = 1,
	evaluateAll = 2,
	export = 3,
}

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
	nextId: number;
	exports: ScopeObject[];
	inIndentation: boolean,
	file: FileContext,
	errors: CompileError[],
	
	compilerStage: CompilerStage,
	disableDependencyEvaluation: boolean,
};

// file is null!
export function newBuilderContext(compilerOptions: CompilerOptions): BuilderContext {
	return {
		compilerOptions: compilerOptions,
		topCodeGenText: getCGText(),
		options: {
			compileTime: true,
			codeGenText: [],
		},
		nextId: 0,
		exports: [],
		inIndentation: true,
		file: null as any,
		errors: [],
		
		compilerStage: CompilerStage.findAliases,
		disableDependencyEvaluation: false,
	};
}

let filesToCompile: FileContext[] = [];

export function resetFilesToCompile() {
	filesToCompile = [];
}

function checkScopeLevel(context: BuilderContext, level: ScopeObject_alias[]) {
	for (let i = 0; i < level.length; i++) {
		const alias = level[i];
		if (alias.isAfield) {
			continue;
		}
		
		if (alias.valueAST) {
			const valueAST = alias.valueAST;
			alias.valueAST = null;
			const scopeList = build(context, [valueAST], {
				codeGenText: null,
				compileTime: true,
			}, null, false, false, "yes");
		}
	}
}

export function compile(context: BuilderContext, onTokens: null | ((tokens: Token[]) => void)) {
	let mainFile: FileContext;
	try {
		context.compilerStage = CompilerStage.findAliases;
		mainFile = compileFile(context, context.compilerOptions.filePath, onTokens);
		
		context.compilerStage = CompilerStage.evaluateAliasDependencies;
		for (let i = 0; i < filesToCompile.length; i++) {
			context.file = filesToCompile[i];
			// context.file.scope.levels[0] = [];
			const scopeList = build(context, filesToCompile[i].AST, {
				codeGenText: null,
				compileTime: true,
			}, null, false, false, "yes");
			context.file.scope.levels[0] = scopeList as ScopeObject_alias[];
		}
		
		context.compilerStage = CompilerStage.evaluateAll;
		for (let i = 0; i < filesToCompile.length; i++) {
			context.file = filesToCompile[i];
			// context.file.scope.levels[0] = [];
			const scopeList = build(context, filesToCompile[i].AST, {
				codeGenText: null,
				compileTime: true,
			}, null, false, false, "yes");
			context.file.scope.levels[0] = scopeList as ScopeObject_alias[];
		}
		
		// context.file = mainFile;
		
		// TODO
		// const checkStart = Date.now();
		// if (context.compilerOptions.check) {
		// 	for (const key in filesToCompile) {
		// 		context.file = filesToCompile[key];
		// 		context.file.scope.currentLevel = 0;
		// 		checkScopeLevel(context, context.file.scope.levels[0]);
		// 	}
		// }
		// logger.addTime("checking", Date.now() - checkStart);
		
		context.compilerStage = CompilerStage.export;
		const exportStart = Date.now();
		codeGen.start(context);
		for (let i = 0; i < context.exports.length; i++) {
			const toExport = context.exports[i];
			if (toExport.kind == "function") {
				callFunction(context, toExport, null, "builtin", false, null, null, null);
			}
		}
		logger.addTime("exporting", Date.now() - exportStart);
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
}

export function compileFile(context: BuilderContext, filePath: string, onTokens: null | ((tokens: Token[]) => void)): FileContext {
	// if the file has already been compiled, there is no point compiling it again
	// (this also lets two files import each other)
	for (let i = 0; i < filesToCompile.length; i++) {
		if (filePath == filesToCompile[i].path) {
			return filesToCompile[i];
		}
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
	filesToCompile.push(newFile);
	
	const text = utilities.readFile(filePath);
	// console.log("text:", text);
	
	const lexStart = Date.now();
	const tokens = lex(filePath, text);
	logger.addTime("lexing", Date.now() - lexStart);
	// console.log("tokens:", tokens);
	
	if (onTokens) {
		onTokens(tokens);
	}
	
	const parseStart = Date.now();
	newFile.AST = parse({
		tokens: tokens,
		i: 0,
	}, ParserMode.normal, null);
	logger.addTime("parsing", Date.now() - parseStart);
	// console.log(`AST '${filePath}':`, JSON.stringify(newFile.AST, undefined, 4));
	
	const scopeList = build(context, newFile.AST, {
		codeGenText: null,
		compileTime: true,
	}, null, false, false, "yes");
	context.file.scope.levels[0] = scopeList as ScopeObject_alias[];
	
	// console.log("scopeList:", JSON.stringify(scopeList, undefined, 4));
	
	context.file = oldFile;
	return newFile;
}