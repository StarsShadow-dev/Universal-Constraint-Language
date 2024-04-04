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

function testSuccess() {
	console.log("\n\t\x1B[32mtest success!\x1B[0m\n");
}

function testFailure(msg: string) {
	console.log(`\n\t\x1B[31mtest failure:\n${msg}\x1B[0m\n`);
	process.exitCode = 1;
}

function testFile(filePath: string) {
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
	if (mode != "compError" && mode != "compPass" && mode != "compOut") {
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
			if (mode == "compPass") {
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
		codeGenText: {},
		filePath: filePath,
		
		scope: {
			levels: [],
			currentLevel: -1,
			visible: [],	
		},
		
		options: {
			doCodeGen: false,
		}
	};

	let scopeList: ScopeObject[] = [];

	try {
		scopeList = build(builderContext, AST, null, null);
	} catch (error) {
		if (error instanceof CompileError) {
			if (mode == "compPass") {
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
	} else if (mode == "compPass") {
		testSuccess();
	} else if (mode == "compOut") {
		const expectedOutput = comments.join("\n");
		const actualOutput = builderContext.codeGenText["top"];
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

testDir("./tests/compPass");
testDir("./tests/compError");
testDir("./tests/compOut");