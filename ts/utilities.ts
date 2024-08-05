import * as fs from "fs";
import { exit } from "process";
import path from "path";

import logger from "./logger";
import { CompileError } from "./report";

export function unreachable(): never {
	debugger;
	console.trace();
	console.error("unreachable code reached");
	exit(1);
}

export function assert(condition: boolean, msg?: string): asserts condition {
	if (!condition) {
		debugger;
		throw msg;
	}
}

export function TODO(): never {
	debugger;
	// console.trace();
	// console.error("TODO reached");
	// exit(1);
	throw "TODO reached";
}

export function byteSize(str: string): number {
	return new Blob([str]).size;
}

export function readFile(filePath: string): string {
	try {
		const text = fs.readFileSync(filePath, { encoding: 'utf8' });
		logger.readFile({
			path: filePath,
			byteSize: byteSize(text),
		});
		return text;
	} catch (error) {
		throw new CompileError(`could not read file at path '${filePath}'`);
	}
}

export function writeFile(filePath: string, text: string) {
	try {
		fs.writeFileSync(path.normalize(filePath), text, { encoding: 'utf8' });
		logger.writeFile({
			path: filePath,
			byteSize: byteSize(text),
		});
	} catch (error) {
		throw new CompileError(`could not write file at path '${filePath}'`);
	}
}