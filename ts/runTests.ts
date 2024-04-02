// tsc -p ./tsconfig.json && node out/runTests.js

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
	console.log("test success!");
}

function testFailure(msg: string) {
	console.log(`test failure:\n${msg}`);
	process.exitCode = 1;
}

function testFile(filePath: string) {
	console.log(`running test: '${filePath}'`);
	
	setUpBuiltin();
	
	const text = utilities.readFile(filePath);

	const tokens = lex(filePath, text);
	// console.log("tokens: ", tokens);
	
	if (!tokens[0] || tokens[0].type != TokenType.comment) {
		throw `unable to test file '${filePath}'`;
	}
	
	let comments = tokens[0].text.split("\n");
	
	let mode = comments.shift();
	if (mode != "compError") {
		throw `mode != "compError" mode = "${mode}"`;
	}

	const AST = parse({
		tokens: tokens,
		i: 0,
	}, ParserMode.normal, null);

	const builderContext: BuilderContext = {
		scopeLevels: [],
		level: -1,
		codeGenText: {},
		filePath: filePath,
	};

	let scopeList: ScopeObject[] = [];

	try {
		scopeList = build(builderContext, AST, null, null);
	} catch (error) {
		if (error instanceof CompileError) {
			const expectedOutput = comments.join("\n");
			const actualOutput = error.getText(false);
			if (expectedOutput == actualOutput) {
				testSuccess();
			} else {
				testFailure(`expectedOutput = ${expectedOutput}\nactualOutput = ${actualOutput}`);
			}
			return;
		} else {
			throw error;
		}
	}
	
	const expectedOutput = comments.join("\n");
	testFailure(`expected error output = ${expectedOutput}\nactual output = success`);
}

function testDir(dirPath: string) {
	const files = fs.readdirSync(dirPath);
	console.log(files);
	files.forEach((file) => {
		testFile(path.join(dirPath, file));
	})
}

testDir("./tests/compError");
