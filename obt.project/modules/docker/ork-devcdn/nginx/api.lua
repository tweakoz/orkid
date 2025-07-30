-- API handler for CDN operations
local cjson = require "cjson"

-- Get request method and endpoint
local method = ngx.req.get_method()
local uri = ngx.var.uri
local args = ngx.req.get_uri_args()

-- Base directory for content
local base_dir = "/cdn/content"

-- Helper function to get file stats
local function get_file_stats()
    -- Use shell command to get file stats
    local cmd = "find " .. base_dir .. " -type f -exec stat -c '%s' {} + 2>/dev/null | awk '{sum+=$1; count++} END {print count \":\" sum}'"
    local handle = io.popen(cmd)
    local result = handle:read("*a")
    handle:close()
    
    local count, size = result:match("(%d+):(%d+)")
    local total_size = tonumber(size) or 0
    local file_count = tonumber(count) or 0
    
    return {
        total_files = file_count,
        total_bytes = total_size,
        total_size_mb = string.format("%.2f", total_size / 1048576)
    }
end

-- Helper function to list all files
local function list_files()
    local files = {}
    
    -- Use shell command to list files with details
    local cmd = "find " .. base_dir .. " -maxdepth 1 -type f -exec stat -c '%n:%s:%Y' {} + 2>/dev/null"
    local handle = io.popen(cmd)
    if handle then
        for line in handle:lines() do
            local path, size, mtime = line:match("([^:]+):(%d+):(%d+)")
            if path then
                local name = path:match(".*/(.+)$") or path
                
                -- Calculate MD5 hash
                local md5_cmd = "md5sum '" .. path:gsub("'", "'\\''") .. "' | cut -d' ' -f1"
                local md5_handle = io.popen(md5_cmd)
                local md5_hash = ""
                if md5_handle then
                    md5_hash = md5_handle:read("*a"):gsub("%s+", "")
                    md5_handle:close()
                end
                
                table.insert(files, {
                    name = name,
                    size = tonumber(size) or 0,
                    modified = tonumber(mtime) or 0,
                    size_mb = string.format("%.2f", (tonumber(size) or 0) / 1048576),
                    md5 = md5_hash
                })
            end
        end
        handle:close()
    end
    
    -- Sort by name
    table.sort(files, function(a, b) return a.name < b.name end)
    
    return files
end

-- API endpoints
if uri == "/api/health" then
    -- Health check endpoint
    if method ~= "GET" then
        ngx.status = 405
        ngx.header["Content-Type"] = "application/json"
        ngx.say(cjson.encode({error = "Method not allowed"}))
        return
    end
    
    ngx.header["Content-Type"] = "application/json"
    ngx.say(cjson.encode({
        status = "healthy",
        version = "1.0.0",
        service = "ork-devcdn",
        timestamp = os.time()
    }))
    
elseif uri == "/api/list" then
    -- List all files
    if method ~= "GET" then
        ngx.status = 405
        ngx.header["Content-Type"] = "application/json"
        ngx.say(cjson.encode({error = "Method not allowed"}))
        return
    end
    
    local files = list_files()
    ngx.header["Content-Type"] = "application/json"
    ngx.say(cjson.encode({
        files = files,
        count = #files,
        timestamp = os.time()
    }))
    
elseif uri == "/api/stats" then
    -- Get CDN statistics
    if method ~= "GET" then
        ngx.status = 405
        ngx.header["Content-Type"] = "application/json"
        ngx.say(cjson.encode({error = "Method not allowed"}))
        return
    end
    
    local stats = get_file_stats()
    stats.timestamp = os.time()
    stats.base_path = base_dir
    
    ngx.header["Content-Type"] = "application/json"
    ngx.say(cjson.encode(stats))
    
elseif uri:match("^/api/fileinfo/") then
    -- Get file info including MD5 for a specific file
    if method ~= "GET" then
        ngx.status = 405
        ngx.header["Content-Type"] = "application/json"
        ngx.say(cjson.encode({error = "Method not allowed"}))
        return
    end
    
    local file_path = uri:match("^/api/fileinfo/(.*)")
    if not file_path or file_path == "" then
        ngx.status = 400
        ngx.header["Content-Type"] = "application/json"
        ngx.say(cjson.encode({error = "Missing file path"}))
        return
    end
    
    -- Security: prevent path traversal
    if file_path:match("%.%.") or file_path:match("^/") or file_path:match("//") then
        ngx.status = 403
        ngx.header["Content-Type"] = "application/json"
        ngx.say(cjson.encode({error = "Invalid file path"}))
        return
    end
    
    local full_path = base_dir .. "/" .. file_path
    
    -- Check if file exists using io.open
    local file = io.open(full_path, "r")
    if not file then
        ngx.status = 404
        ngx.header["Content-Type"] = "application/json"
        ngx.say(cjson.encode({error = "File not found"}))
        return
    end
    file:close()
    
    -- Get file stats using stat command
    local stat_cmd = "stat -c '%s:%Y' '" .. full_path:gsub("'", "'\\''") .. "' 2>/dev/null"
    local stat_handle = io.popen(stat_cmd)
    local stat_result = stat_handle:read("*a")
    stat_handle:close()
    
    local size, mtime = stat_result:match("(%d+):(%d+)")
    local attr = {
        size = tonumber(size) or 0,
        modification = tonumber(mtime) or 0
    }
    
    -- Calculate MD5 hash with error handling
    local md5_hash = ""
    local success, err = pcall(function()
        -- Escape single quotes in file path
        local escaped_path = full_path:gsub("'", "'\\''")
        local md5_cmd = "md5sum '" .. escaped_path .. "' 2>&1 | cut -d' ' -f1"
        
        -- Log the command for debugging
        ngx.log(ngx.INFO, "Executing MD5 command: ", md5_cmd)
        
        local handle = io.popen(md5_cmd)
        if handle then
            local output = handle:read("*a")
            handle:close()
            
            if output then
                md5_hash = output:gsub("%s+", "")
                ngx.log(ngx.INFO, "MD5 hash calculated: ", md5_hash)
            else
                ngx.log(ngx.ERR, "MD5 command returned no output")
            end
        else
            ngx.log(ngx.ERR, "Failed to execute MD5 command")
        end
    end)
    
    if not success then
        ngx.log(ngx.ERR, "Error calculating MD5: ", err)
        -- Continue with empty MD5 rather than failing the request
    end
    
    ngx.header["Content-Type"] = "application/json"
    ngx.say(cjson.encode({
        name = file_path,
        size = attr.size,
        modified = attr.modification,
        md5 = md5_hash,
        exists = true
    }))
    
else
    -- Unknown endpoint
    ngx.status = 404
    ngx.header["Content-Type"] = "application/json"
    ngx.say(cjson.encode({
        error = "Not found",
        message = "Unknown API endpoint",
        uri = uri
    }))
end