import * as fs from "fs";
import path from "path";

import utilities from "./utilities";
import { TokenKind } from "./types";
import {
	CompilerOptions,
	compile,
	newBuilderContext,
	resetFilesToCompile
} from "./compiler";

const c_green = "\x1B[32m";
const c_red = "\x1B[31m"
const c_reset = "\x1B[0m";

let total = 0;
let skipped = 0;
let succeeded = 0;
let failed = 0;

function testSkip() {
	console.log(`\n\t\x1B[34mTest skipped.${c_reset}\n`);
	skipped++;
}

function testSuccess() {
	console.log(`\n\t${c_green}Test success!${c_reset}\n`);
	succeeded++;
}

function testFailure(msg: any) {
	console.log(`\n\t${c_red}Test failure:\n${msg}${c_reset}\n`);
	failed++;
	
	process.exitCode = 1;
}

function testFile(filePath: string) {
	filePath = path.normalize(filePath);
	
	total++;
	
	console.log(`running test: '${filePath}'`);
	
	resetFilesToCompile();
	
	let comments: any;
	let mode: any;
	
	const options: CompilerOptions = {
		filePath: filePath,
		fancyErrors: false,
		builderTransforms: {
			removeTypes: true,
		}
	};
	
	const context = newBuilderContext(options);
	
	try {
		compile(context, (tokens) => {
			if (!tokens[0] || tokens[0].type != TokenKind.comment) {
				console.log(`unable to test file '${filePath}' (no top comment)`);
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
	} catch (error) {
		if (error == "__testSkip__") {
			testSkip();
			return;
		}
		
		testFailure(error);
		return;
	}
	
	if (context.errors.length == 0) {
		if (mode == "compError") {
			const expectedOutput = comments.join("\n");
			testFailure(`expected error output = ${expectedOutput}\nactual output is success`);
		} else if (mode == "compSucceed") {
			testSuccess();
		} else if (mode == "compOut") {
			throw "compOut";
			// const expectedOutput = comments.join("\n");
			// const actualOutput = context.topCodeGenText.join("");
			// if (expectedOutput == actualOutput) {
			// 	testSuccess();
			// } else {
			// 	testFailure(`expectedOutput = ${expectedOutput}\nactualOutput = ${actualOutput}`);
			// }
		}
	} else {
		let errorText = "";
		for (const error of context.errors) {
			errorText += error.getText(false);
		}
		
		if (mode == "compSucceed") {
			testFailure(`compilation failed, output = ${errorText}`);
		} else {
			const expectedOutput = comments.join("\n");
			const actualOutput = errorText;
			if (expectedOutput == actualOutput) {
				testSuccess();
			} else {
				testFailure(`expectedOutput = ${expectedOutput}\nactualOutput = ${actualOutput}`);
			}
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
// testDir("./tests/compOut");
// testDir("./tests/js");

console.log(`total: ${total}`);
console.log(`skipped: ${skipped}`);
console.log(`succeeded: ${c_green}${succeeded}${c_reset}`);
console.log(`failed: ${c_red}${failed}${c_reset}`);