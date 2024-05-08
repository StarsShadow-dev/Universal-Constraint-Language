import * as fs from "fs";
import { exit } from "process";
import { CompileError } from "./report";
import logger from "./logger";

const utilities = {
	unreachable(): never {
		debugger;
		console.error("unreachable code reached");
		exit(1);
	},
	
	TODO(): never {
		debugger;
		console.error("TODO reached");
		exit(1);
	},
	
	byteSize(str: string): number {
		return new Blob([str]).size;
	},
	
	readFile(path: string): string {
		try {
			const text = fs.readFileSync(path, { encoding: 'utf8' });
			logger.readFile({
				path: path,
				byteSize: utilities.byteSize(text),
			});
			return text;
		} catch (error) {
			throw new CompileError(`could not read file at path '${path}'`);
		}
	},
}

export default utilities;