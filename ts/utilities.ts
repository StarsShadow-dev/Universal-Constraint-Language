import * as fs from "fs";
import { exit } from "process";
import { CompileError } from "./report";
import logger from "./logger";
import path from "path";

const utilities = {
	unreachable(): never {
		debugger;
		console.error("unreachable code reached");
		exit(1);
	},
	
	assert(condition: boolean, msg?: string): asserts condition {
		if (!condition) {
			debugger;
			throw msg;
		}
	},
	
	TODO(): never {
		debugger;
		console.error("TODO reached");
		exit(1);
	},
	
	byteSize(str: string): number {
		return new Blob([str]).size;
	},
	
	readFile(filePath: string): string {
		try {
			const text = fs.readFileSync(filePath, { encoding: 'utf8' });
			logger.readFile({
				path: filePath,
				byteSize: utilities.byteSize(text),
			});
			return text;
		} catch (error) {
			throw new CompileError(`could not read file at path '${filePath}'`);
		}
	},
	
	writeFile(filePath: string, text: string) {
		try {
			fs.writeFileSync(path.normalize(filePath), text, { encoding: 'utf8' });
			logger.writeFile({
				path: filePath,
				byteSize: utilities.byteSize(text),
			});
		} catch (error) {
			throw new CompileError(`could not write file at path '${filePath}'`);
		}
	},
}

export default utilities;