#import "@preview/shadowed:0.2.0": shadowed

#import "definitions.typ": *
#import "theorems.typ": *

// environments
#let callout(
	breakable: true,
	headless: false,
	header-inset: 0.5em,
	width: auto,
	stroke-width: 3pt,
	border-radius: 0pt,
	border-width: 0pt,
	content-inset: 1em,
	header-color: black,
	header-text-color: white,
	accent-color: black,
	body-color: none,
	clip-content: false,
	shadow: false,
	shadow-offset: 0.4em,
	title: none,
	icon: none,
	content,
) = context {
	let body-color = if body-color != none {
		body-color
	} else {
		if accent-color == black {
			rgb("#f6f8f9")
		} else {
			color.mix((accent-color, 10%), white)
		}
	}
	let stroke-color = accent-color
	let border-color = accent-color

	let header_block = block(
		sticky: true,
		below: 0pt,
		fill: header-color,
		width: 100%,
		radius: (
			top-right: border-radius,
			top-left: if (stroke-width == 0pt) { border-radius } else { 0pt },
		),
		inset: header-inset,
	)[
		#let args = arguments(fill: header-text-color, weight: "bold", title)
		#if icon == none {
			text(..args)
		} else {
			grid(
				columns: (auto, auto),
				align: (horizon, left + horizon),
				gutter: 1em,
				box(height: 1em)[ #icon ], text(..args),
			)
		}
	]

	let content_block(content) = block(
		breakable: breakable,
		width: 100%,
		fill: body-color,
		inset: content-inset,
		radius: (
			top-right: if (title != none) { 0pt } else { border-radius },
			top-left: if (title != none or stroke-width != 0pt) { 0pt } else {
				border-radius
			},
			bottom-left: if (stroke-width == 0pt) { border-radius } else { 0pt },
			bottom-right: border-radius,
		),
		above: 0pt,
	)[#content]

	let callout = block(
		breakable: breakable,
		width: width,
		radius: (
			right: border-radius,
			left: if (stroke-width == 0pt) {
				border-radius
			} else { 0pt },
		),
		stroke: (
			left: if (stroke-width == 0pt) {
				border-width + border-color
			} else { (thickness: stroke-width, paint: stroke-color, cap: "butt") },
			top: if (title != none) { border-width + header-color } else {
				border-width + border-color
			},
			rest: border-width + border-color,
		),
		clip: clip-content,
	)[
		#set align(start)
		#if (not headless and title != none) { header_block }
		#content_block(content)
	]

	if shadow {
		return shadowed(
			radius: border-radius,
			shadow: 0pt,
			dx: shadow-offset,
			dy: shadow-offset,
			callout,
		)
	} else {
		return callout
	}
}

#let thm-plain(
	env_name,
	linebreak: false,
	style: "normal",
	box: false,
	display-title: false,
	..calloutargs,
) = thmenv(
	env_name,
	"heading",
	none,
	(
		name,
		number,
		body,
		color: calloutargs.at("accent-color", default: black),
		linebreak: linebreak,
		style: style,
		box: box,
		display-title: display-title,
		..additional_calloutargs,
	) => [
		#if display-title {
			callout(
				..calloutargs,
				..additional_calloutargs,
				title: get_env_name(env_name)
					+ if number != none { " " + number }
					+ if type(name) == str or type(name) == content {
						if type(name) == str and name.starts-with(regex("https?://")) {
							name = link(name, text(fill: white, name))
						}
						" (" + name + ")"
					},
				body,
			)
		} else {
			let header = [
				#{
					let color = color.mix((color, 80%), black)
					let args = arguments(weight: "bold", style: style, fill: color)

					text(
						..args,
						get_env_name(env_name),
					)
					if number != none {
						text(..args, " " + number)
					}
					if type(name) == str or type(name) == content {
						text(style: style, fill: color, " (" + name + ")")
					}
					if linebreak {
						[\ ]
					} else {
						text(..args, ": ")
					}
				}
			]
			if box {
				callout(
					..calloutargs,
					..additional_calloutargs,
					header + body,
				)
			} else {
				header + body
			}
		}
	],
).with(supplement: get_env_name(env_name))

#let thm-box(
	env_name,
	..thmboxargs,
	accent: color.black,
	linebreak: false,
) = thmbox(
	env_name,
	get_env_name(env_name),
	supplement: get_env_name(env_name),
	// base = if (counter(heading).get().at(1)) { "heading" } else { none },
	titlefmt: (content, delta: none) => context {
		text(weight: "bold", fill: accent, content)
	},
	namefmt: content => text(
		fill: accent,
		"(" + content + ")",
	),
	separator: text(
		weight: "bold",
		fill: accent,
		if (linebreak) { [\ ] } else { [:] },
	),
	breakable: true,
	..thmboxargs,
)

// predefined environments
#let thm_numbering(..args) = {
	if (args.at(2, default: none) == none and args.at(0) == 0) {
		return args.at(1)
	} else {
		return numbering("1.1", ..args)
	}
}

#let un-numbered(base) = (
	base.with(numbering: thm_numbering),
	base.with(numbering: none),
)

#let proof = thmproof("proof", get_env_name("proof"))
#let solution = thmproof("solution", get_env_name("solution"))

#let (definition, _definition) = un-numbered(thm-plain("definition"))
#let (notation, _notation) = un-numbered(thm-plain("notation"))
#let (conjecture, _conjecture) = un-numbered(thm-plain("conjecture", style: "italic"))
#let (remark, _remark) = un-numbered(thm-plain("remark", box: true))
#let (exercise, _exercise) = un-numbered(thm-plain("exercise", box: true))
#let (question, _question) = un-numbered(thm-plain("question", box: true))
#let (problem, _problem) = un-numbered(thm-plain(
	"problem",
	display-title: true,
	box: true,
	border-radius: 6pt,
	stroke-width: 0pt,
	border-width: 0.7pt,
	body-color: rgb("#f2ecf5"),
	header-color: rgb("#5c5477"),
	shadow: true,
))
#let (theorem, _theorem) = un-numbered(thm-box("theorem", linebreak: true, ..colors.env.theorems))
#let (corollary, _corollary) = un-numbered(thm-box("corollary", ..colors.env.theorems))
#let (proposition, _proposition) = un-numbered(thm-box("proposition", linebreak: true, ..colors.env.theorems))
#let (lemma, _lemma) = un-numbered(thm-plain("lemma", box: true, accent-color: rgb("#009a55")))
#let (example, _example) = un-numbered(thm-box("example", radius: 0em, ..colors.env.examples))

#let reformulation = thm-plain("reformulation", box: true, accent-color: rgb("#009a55")).with(numbering: none)
#let algorithm = thm-plain("algorithm", box: true, accent-color: rgb("#8839ee")).with(numbering: none)

#let eqn(it) = {
	set math.equation(numbering: "(1)")
	it
}
