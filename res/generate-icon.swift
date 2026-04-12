// generate-icon.swift
//
// Draws the namp macOS icon (1024x1024 PNG with alpha) following the
// Big Sur+ icon grid: 824x824 body centered in 1024 canvas (100px inset),
// corner radius 185.4.
//
// Run: swift res/generate-icon.swift res/namp.png

import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers
import CoreText

let canvas: CGFloat = 1024
let bodyInset: CGFloat = 100
let bodySize: CGFloat = canvas - 2 * bodyInset   // 824
let bodyRadius: CGFloat = 185.4

let colorSpace = CGColorSpaceCreateDeviceRGB()
guard let ctx = CGContext(
  data: nil,
  width: Int(canvas),
  height: Int(canvas),
  bitsPerComponent: 8,
  bytesPerRow: 0,
  space: colorSpace,
  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
) else {
  fatalError("failed to create CGContext")
}

// Flip so (0,0) is top-left to match SVG-style coordinates.
ctx.translateBy(x: 0, y: canvas)
ctx.scaleBy(x: 1, y: -1)

// Start fully transparent.
ctx.clear(CGRect(x: 0, y: 0, width: canvas, height: canvas))

// Body: rounded square with dark-blue vertical gradient.
let bodyRect = CGRect(x: bodyInset, y: bodyInset, width: bodySize, height: bodySize)
let bodyPath = CGPath(roundedRect: bodyRect, cornerWidth: bodyRadius, cornerHeight: bodyRadius, transform: nil)

ctx.saveGState()
ctx.addPath(bodyPath)
ctx.clip()
let bgColors = [
  CGColor(srgbRed: 0.090, green: 0.141, blue: 0.255, alpha: 1.0), // #173A66-ish top
  CGColor(srgbRed: 0.016, green: 0.031, blue: 0.078, alpha: 1.0), // #040814 bottom
] as CFArray
let bgLocations: [CGFloat] = [0.0, 1.0]
let bgGradient = CGGradient(colorsSpace: colorSpace, colors: bgColors, locations: bgLocations)!
ctx.drawLinearGradient(
  bgGradient,
  start: CGPoint(x: 0, y: bodyInset),
  end: CGPoint(x: 0, y: bodyInset + bodySize),
  options: []
)
ctx.restoreGState()

// Spectrum bars.
let barCount = 7
let barWidth: CGFloat = 72
let barGap: CGFloat = 18
let barsTotalWidth = CGFloat(barCount) * barWidth + CGFloat(barCount - 1) * barGap
let barsStartX = (canvas - barsTotalWidth) / 2
let barBaselineY: CGFloat = 760
let barHeights: [CGFloat] = [220, 320, 420, 520, 450, 350, 250]
let barRadius: CGFloat = 10

let barGradientColors = [
  CGColor(srgbRed: 0.00, green: 0.90, blue: 0.46, alpha: 1.0), // #00E676
  CGColor(srgbRed: 1.00, green: 0.92, blue: 0.23, alpha: 1.0), // #FFEB3B
  CGColor(srgbRed: 1.00, green: 0.24, blue: 0.00, alpha: 1.0), // #FF3D00
] as CFArray
let barGradientLocations: [CGFloat] = [0.0, 0.5, 1.0]
let barGradient = CGGradient(colorsSpace: colorSpace, colors: barGradientColors, locations: barGradientLocations)!

for i in 0..<barCount {
  let x = barsStartX + CGFloat(i) * (barWidth + barGap)
  let height = barHeights[i]
  let y = barBaselineY - height
  let rect = CGRect(x: x, y: y, width: barWidth, height: height)
  let path = CGPath(roundedRect: rect, cornerWidth: barRadius, cornerHeight: barRadius, transform: nil)

  ctx.saveGState()
  ctx.addPath(path)
  ctx.clip()
  // Each bar: green at bottom → red at top, within its own bounds.
  ctx.drawLinearGradient(
    barGradient,
    start: CGPoint(x: 0, y: y),
    end: CGPoint(x: 0, y: y + height),
    options: []
  )
  ctx.restoreGState()
}

// Subtle separator line between bars and text.
ctx.setStrokeColor(CGColor(srgbRed: 0.110, green: 0.208, blue: 0.376, alpha: 1.0))
ctx.setLineWidth(6)
ctx.move(to: CGPoint(x: barsStartX, y: barBaselineY + 20))
ctx.addLine(to: CGPoint(x: barsStartX + barsTotalWidth, y: barBaselineY + 20))
ctx.strokePath()

// "namp" wordmark.
let fontSize: CGFloat = 150
let fontDesc = CTFontDescriptorCreateWithNameAndSize("HelveticaNeue-Bold" as CFString, fontSize)
let font = CTFontCreateWithFontDescriptor(fontDesc, fontSize, nil)
let textColor = CGColor(srgbRed: 1.0, green: 1.0, blue: 1.0, alpha: 1.0)
let attrs: [CFString: Any] = [
  kCTFontAttributeName: font,
  kCTForegroundColorAttributeName: textColor,
  kCTKernAttributeName: 4
]
let attrString = NSAttributedString(string: "namp", attributes: attrs as [NSAttributedString.Key: Any])
let line = CTLineCreateWithAttributedString(attrString)
let bounds = CTLineGetBoundsWithOptions(line, .useGlyphPathBounds)
let textX = (canvas - bounds.width) / 2 - bounds.origin.x
let textBaselineY: CGFloat = 880

ctx.saveGState()
// Context has a y-down (SVG-style) flip; textMatrix un-flips glyphs.
ctx.textMatrix = CGAffineTransform(scaleX: 1, y: -1)
ctx.textPosition = CGPoint(x: textX, y: textBaselineY)
CTLineDraw(line, ctx)
ctx.restoreGState()

// Write PNG.
guard let image = ctx.makeImage() else { fatalError("makeImage failed") }

let outPath = CommandLine.arguments.count >= 2
  ? CommandLine.arguments[1]
  : "namp.png"
let outURL = URL(fileURLWithPath: outPath)

guard let dest = CGImageDestinationCreateWithURL(outURL as CFURL, UTType.png.identifier as CFString, 1, nil) else {
  fatalError("failed to create PNG destination at \(outPath)")
}
CGImageDestinationAddImage(dest, image, nil)
guard CGImageDestinationFinalize(dest) else {
  fatalError("failed to write PNG")
}
print("wrote \(outPath) (\(Int(canvas))x\(Int(canvas)))")
