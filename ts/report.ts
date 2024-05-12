import { CompilerOptions } from "./compiler";
import { SourceLocation } from "./types";
import utilities from "./utilities";

const lineNumberPadding = 4;
const indicatorTextWindowSize = 2;

export type Indicator = {
	location: SourceLocation,
	msg: string,
}

// TODO: This can read the same file twice, if there are two indicators in a file.
export function getIndicator(indicator: Indicator, fancyIndicators: boolean): string {
	let errorText = "";
	
	if (!fancyIndicators) {
		if (indicator.location != "builtin") {
			errorText += `${indicator.location.path}:${indicator.location.line} -> ${indicator.msg}`;
			return errorText;
		} else {
			errorText += `builtin -> ${indicator.msg}`;
			return errorText;
		}
	}
	
	if (indicator.location != "builtin") {
		const text = utilities.readFile(indicator.location.path);
		// console.log("compileError location:", location);
		
		errorText += `at ${indicator.location.path}:${indicator.location.line}\n`;
		
		let i = 0;
		let line = 1;
		
		function next(): string {
			if (text[i] == "\n") {
				line++;
			}
			return text[i++];
		}
		
		function writeLine(): number {
			let size = 0
			let lineText = "";
			if (indicator.location != "builtin") {
				lineText += line.toString().padStart(lineNumberPadding, "0");
				lineText += " |";
				size = lineText.length;
				
				let lineI = 0;
				while (i < text.length) {
					const char = next();
					
					lineI++;
					
					if (char == "\n") {
						break;
					}
					
					if (char == "\t") {
						lineText += "    ";
					} else {
						lineText += char;
					}
					
					if (lineI == indicator.location.startColumn) {
						size = lineText.length - 1;
					}
				}
				lineText += "\n";
			} else {
				utilities.unreachable();
			}
			errorText += lineText;
			
			return size;
		}
		
		while (i < text.length) {
			if (line == indicator.location.line) {
				const size = writeLine();
				for (let index = 0; index < size; index++) {
					errorText += " ";
				}
				for (let index = 0; index < indicator.location.endColumn - indicator.location.startColumn + 1; index++) {
					errorText += "^";
				}
				errorText += ` ${indicator.msg}\n`;
			} else if (
				line > indicator.location.line - indicatorTextWindowSize - 1 &&
				line < indicator.location.line + indicatorTextWindowSize + 1
			) {
				writeLine();
			} else {
				next();
			}
		}
		
		errorText += `\n`;	
	} else {
		errorText += `at builtin\n\n`;
	}
	
	return errorText;
}

export function printErrors(options: CompilerOptions, errors: CompileError[]) {
	if (options.ideOptions && options.ideOptions.mode == "compileFile") {
		console.log(JSON.stringify(errors));
	} else {
		for (const error of errors) {
			console.log(error.getText(true));
		}
	}
}

export class CompileError {
	msg: string
	private indicators: Indicator[]
	
	constructor(msg: string) {
		this.msg = msg;
		this.indicators = []
	}
	
	public indicator(location: SourceLocation, msg: string): CompileError {
		this.indicators.push({
			location: location,
			msg: msg,
		});
		return this;
	}
	
	public getText(fancyIndicators: boolean): string {
		let text = `error: ${this.msg}\n`;
		for (let i = 0; i < this.indicators.length; i++) {
			const indicator = this.indicators[i];
			text += getIndicator(indicator, fancyIndicators);
			if (!fancyIndicators && this.indicators[i + 1]) {
				text += "\n";
			}
		}
		return text;
	}
}