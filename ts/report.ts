import { SourceLocation } from "./types";
import utilities from "./utilities";

const lineNumberPadding = 4;

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
		
		errorText += `at ${indicator.location.path}:${indicator.location.line}\n\n`;
		
		let i = 0;
		let line = 1;
		
		function writeLine() {
			if (indicator.location != "builtin") {
				errorText += line.toString().padStart(lineNumberPadding, "0");
				errorText += " |";
				for (; i < text.length; i++) {
					if (text[i] == "\n") line++;
					
					if (line != indicator.location.line) continue;
					
					errorText += text[i];
				}
				errorText += "\n";
			} else {
				utilities.unreachable();
			}
		}
		
		for (; i < text.length; i++) {
			if (text[i] == "\n") line++;
			
			if (line == indicator.location.line) {
				if (text[i] == "\n") i++;
				writeLine();
				errorText += `${indicator.msg}\n`;
			}
		}
		
		errorText += `\n`;	
	} else {
		errorText += `at builtin\n\n`;
	}
	
	return errorText;
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