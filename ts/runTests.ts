import * as fs from "fs";

import { TokenType } from "./types";
import utilities from "./utilities";
import { CompileError } from "./report";
import { setUpBuiltin } from "./builtin";
import path from "path";
import { compile, resetCompiledFiles } from "./compiler";

const c_green = "\x1B[32m";
const c_red = "\x1B[31m"
const c_reset = "\x1B[0m";

let total = 0;
let skipped = 0;
let succeeded = 0;
let failed = 0;

function testSkip() {
	console.log(`\n\ttest skipped\n`);
	skipped++;
}

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
	filePath = path.normalize(filePath);
	
	total++;
	
	console.log(`running test: '${filePath}'`);
	
	resetCompiledFiles();
	setUpBuiltin(true);
	
	let comments: any;
	let mode: any;
	
	try {
		const context = compile(filePath, (tokens) => {
			if (!tokens[0] || tokens[0].type != TokenType.comment) {
				console.log(`unable to test file '${filePath}'`);
				utilities.unreachable();
			}
			
			comments = tokens[0].text.split("\n");
			mode = comments.shift();
			if (mode == "testSkip") {
				throw `__testSkip__`;
			}
			if (mode != "compError" && mode != "compSucceed" && mode != "compOut") {
				throw `unknown mode "${mode}"`;
			}
		});
		
		if (mode == "compError") {
			const expectedOutput = comments.join("\n");
			testFailure(`expected error output = ${expectedOutput}\nactual output = success`);
		} else if (mode == "compSucceed") {
			testSuccess();
		} else if (mode == "compOut") {
			const expectedOutput = comments.join("\n");
			const actualOutput = context.topCodeGenText.join("");
			if (expectedOutput == actualOutput) {
				testSuccess();
			} else {
				testFailure(`expectedOutput = ${expectedOutput}\nactualOutput = ${actualOutput}`);
			}
		}
	} catch (error) {
		if (error == "__testSkip__") {
			testSkip();
			return;
		}
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

console.log(`total: ${total}`);
console.log(`skipped: ${skipped}`);
console.log(`succeeded: ${c_green}${succeeded}${c_reset}`);
console.log(`failed: ${c_red}${failed}${c_reset}`);