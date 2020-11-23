hw_breakpoint = {
	source = path.join(dependencies.basePath, "hw-breakpoint"),
}

function hw_breakpoint.import()
	links { "hw-breakpoint" }
	hw_breakpoint.includes()
end

function hw_breakpoint.includes()
	includedirs {
		hw_breakpoint.source
	}
end

function hw_breakpoint.project()
	project "hw-breakpoint"
		language "C++"

		hw_breakpoint.includes()

		files {
			path.join(hw_breakpoint.source, "*.h"),
			path.join(hw_breakpoint.source, "*.cpp"),
		}

		warnings "Off"
		kind "StaticLib"
end

table.insert(dependencies, hw_breakpoint)
