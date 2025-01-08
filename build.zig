const std = @import("std");

// zig fmt: off

// Although this function looks imperative, note that its job is to
// declaratively construct a build graph that will be executed by an external
// runner.
pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "tensionchess",
        .root_source_file = null,
        .target = target,
        .optimize = optimize,
    });

    exe.linkLibCpp();
    try addSources("./src", b, exe);

    b.installArtifact(exe);

    // This *creates* a Run step in the build graph, to be executed when another
    // step is evaluated that depends on it. The next line below will establish
    // such a dependency.
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    // This allows the user to pass arguments to the application in the build
    // command itself, like this: `zig build run -- arg1 arg2 etc`
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}

const standardFlags = [_][]const u8 {
    // compiler options
    "-std=c++23",
    "-O0",
    // "-O3",

    // constexpr
    "-fconstexpr-depth=9999", "-fconstexpr-steps=99999999",

    // error/warning settings
    "-Wno-deprecated-enum-enum-conversion", "-Wunused-value", "-Wno-deprecated-anon-enum-enum-conversion",
};

/// Add all source files from the given directory to the compilation step
pub fn addSources(directory: []const u8, b: *std.Build, c: *std.Build.Step.Compile) !void {
    var dir = try std.fs.cwd().openDir(directory, .{ .iterate = true });
    var walker = try dir.walk(b.allocator);
    defer walker.deinit();

    // all extensions treated as source files
    const sourceExts = [_][]const u8{ ".cc", ".cpp", ".c", ".cxx" };
    // const trackExts = [_][]const u8{ ".h", ".hh", ".hpp", ".hxx" };

    while (try walker.next()) |entry| {
        // check if extension matches one in the list
        const ext = std.fs.path.extension(entry.basename);
        const inclAsSourceFile = for (sourceExts) |e| {
            if (std.mem.eql(u8, ext, e))
                break true;
        } else false;

        // add source file
        if (inclAsSourceFile) {
            // build flags
            var flags: std.ArrayList([]const u8) = std.ArrayList([]const u8).init(b.allocator);
            try flags.appendSlice(&standardFlags);

            // add source file
            c.addCSourceFile(.{ .file = b.path(try std.fs.path.resolve(b.allocator, &.{ "src", entry.path })), .flags = try flags.toOwnedSlice()});
            defer flags.deinit();
        }
    }
}
