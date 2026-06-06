#!/usr/bin/env swift
// Renders the 📖 emoji at every required macOS icon size and writes
// the PNG files into the <outputDir> iconset folder.
// Usage: swift gen_icon.swift <outputDir>
import AppKit
import Foundation

guard CommandLine.arguments.count == 2 else {
    fputs("Usage: gen_icon.swift <iconset-dir>\n", stderr)
    exit(1)
}

let outputDir = CommandLine.arguments[1]

let sizes: [(filename: String, px: Int)] = [
    ("icon_16x16",      16),
    ("icon_16x16@2x",   32),
    ("icon_32x32",      32),
    ("icon_32x32@2x",   64),
    ("icon_128x128",    128),
    ("icon_128x128@2x", 256),
    ("icon_256x256",    256),
    ("icon_256x256@2x", 512),
    ("icon_512x512",    512),
    ("icon_512x512@2x", 1024),
]

for (filename, px) in sizes {
    let size = CGFloat(px)
    let img = NSImage(size: NSSize(width: size, height: size), flipped: false) { rect in
        // Clear to transparent.
        NSColor.clear.set()
        rect.fill()

        let font = NSFont.systemFont(ofSize: size * 0.80)
        let attrs: [NSAttributedString.Key: Any] = [.font: font]
        let str = NSAttributedString(string: "📖", attributes: attrs)
        let textSize = str.size()
        let x = (size - textSize.width)  / 2
        let y = (size - textSize.height) / 2
        str.draw(at: NSPoint(x: x, y: y))
        return true
    }

    guard let tiff   = img.tiffRepresentation,
          let bitmap = NSBitmapImageRep(data: tiff),
          let png    = bitmap.representation(using: .png, properties: [:])
    else {
        fputs("Failed to render \(filename).png\n", stderr)
        exit(1)
    }

    let path = "\(outputDir)/\(filename).png"
    do {
        try png.write(to: URL(fileURLWithPath: path))
    } catch {
        fputs("Could not write \(path): \(error)\n", stderr)
        exit(1)
    }
}
print("Icon PNGs written to \(outputDir)")
