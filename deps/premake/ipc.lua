ipc = {
	source = path.join(dependencies.basePath, "ipc"),
}

function ipc.import()
	ipc.includes()
end

function ipc.includes()
	includedirs {
		ipc.source
	}
end

function ipc.project()

end

table.insert(dependencies, ipc)
