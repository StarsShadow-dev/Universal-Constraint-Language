import * as fs from "fs";

import { ScopeObject, TokenType } from "./types";
import { lex } from "./lexer";
import {
	ParserMode,
	parse,
} from './parser';
import { BuilderContext, build } from "./builder";
import utilities from "./utilities";
import { CompileError } from "./report";
import { setUpBuiltin } from "./builtin";
import path from "path";
import codeGen from "./codeGen";

const c_green = "\x1B[32m";
const c_red = "\x1B[31m"
const c_reset = "\x1B[0m";

let total = 0;
let succeeded = 0;
let failed = 0;

function testSuccess() {
	console.log(`\n\t${c_green}test success!${c_reset}\n`);
	succeeded++;
}

function testFailure(msg: string) {
	console.log(`\n\t${c_red}test failure:\n${msg}${c_reset}\n`);
	failed++;
	
	process.exitCode = 1;
}

function testFile(filePath: string) {
	total++;
	
	console.log(`running test: '${filePath}'`);
	
	setUpBuiltin(true);
	
	const text = utilities.readFile(filePath);

	const tokens = lex(filePath, text);
	// console.log("tokens: ", tokens);
	
	if (!tokens[0] || tokens[0].type != TokenType.comment) {
		throw `unable to test file '${filePath}'`;
	}
	
	let comments = tokens[0].text.split("\n");
	let mode = comments.shift();
	if (mode != "compError" && mode != "compSucceed" && mode != "compOut") {
		throw `unknown mode "${mode}"`;
	}
	
	let AST;
	
	try {
		AST = parse({
			tokens: tokens,
			i: 0,
		}, ParserMode.normal, null);
	} catch (error) {
		if (error instanceof CompileError) {
			if (mode == "compSucceed") {
				testFailure(`compilation failed, output = ${error.getText(false)}`);
			} else {
				const expectedOutput = comments.join("\n");
				const actualOutput = error.getText(false);
				if (expectedOutput == actualOutput) {
					testSuccess();
				} else {
					testFailure(`expectedOutput = ${expectedOutput}\nactualOutput = ${actualOutput}`);
				}
			}
			
			return;
		} else {
			throw error;
		}
	}

	const builderContext: BuilderContext = {
		topCodeGenText: [""],
		
		filePath: filePath,
		
		scope: {
			levels: [],
			currentLevel: -1,
			function: null,
			generatingFunction: null,
		},
		
		options: {
			compileTime: true,
			codeGenText: [],
			disableValueEvaluation: false,
		},
		nextSymbolName: 0,
	};

	let scopeList: ScopeObject[] = [];

	try {
		scopeList = build(builderContext, AST, null, null, false);
	} catch (error) {
		if (error instanceof CompileError) {
			if (mode == "compSucceed") {
				testFailure(`compilation failed, output = ${error.getText(false)}`);
			} else {
				const expectedOutput = comments.join("\n");
				const actualOutput = error.getText(false);
				if (expectedOutput == actualOutput) {
					testSuccess();
				} else {
					testFailure(`expectedOutput = ${expectedOutput}\nactualOutput = ${actualOutput}`);
				}
			}
			
			return;
		} else {
			throw error;
		}
	}
	
	if (mode == "compError") {
		const expectedOutput = comments.join("\n");
		testFailure(`expected error output = ${expectedOutput}\nactual output = success`);
	} else if (mode == "compSucceed") {
		testSuccess();
	} else if (mode == "compOut") {
		const expectedOutput = comments.join("\n");
		const actualOutput = builderContext.topCodeGenText.join("");
		if (expectedOutput == actualOutput) {
			testSuccess();
		} else {
			testFailure(`expectedOutput = ${expectedOutput}\nactualOutput = ${actualOutput}`);
		}
	}
}

function testDir(dirPath: string) {
	const files = fs.readdirSync(dirPath);
	// console.log(files);
	files.forEach((file) => {
		testFile(path.join(dirPath, file));
	})
}

testDir("./tests/compSucceed");
testDir("./tests/compError");
testDir("./tests/compOut");
testDir("./tests/js");

console.log(`total ${total}`);
console.log(`succeeded ${c_green}${succeeded}${c_reset}`);
console.log(`failed ${c_red}${failed}${c_reset}`);