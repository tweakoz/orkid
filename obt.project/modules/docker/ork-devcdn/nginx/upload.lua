local function handle_upload()
    -- Get request info
    local uri = ngx.var.request_uri
    local client_ip = ngx.var.remote_addr
    local api_key = ngx.var.http_x_api_key or ngx.var.arg_key
    local start_time = ngx.now()
    
    -- Get the file path from the URI
    local file_path = uri:match("^/upload/(.*)")
    
    ngx.log(ngx.INFO, "Upload started: ", client_ip, " -> ", file_path or "unknown", 
            " key:", api_key and string.sub(api_key, 1, 8) .. "..." or "none")
    
    if not file_path or file_path == "" then
        ngx.log(ngx.WARN, "Upload failed - missing path: ", client_ip, " ", uri)
        ngx.status = 400
        ngx.header["Content-Type"] = "application/json"
        ngx.say('{"error": "Missing file path"}')
        ngx.exit(400)
    end
    
    -- Security: prevent path traversal
    if file_path:match("%.%.") or file_path:match("^/") or file_path:match("//") then
        ngx.log(ngx.WARN, "Upload blocked - path traversal: ", client_ip, " ", file_path)
        ngx.status = 403
        ngx.header["Content-Type"] = "application/json"
        ngx.say('{"error": "Invalid file path"}')
        ngx.exit(403)
    end
    
    -- Construct full file path
    local full_path = "/cdn/content/" .. file_path
    
    ngx.log(ngx.INFO, "Upload target: ", client_ip, " -> ", full_path)
    
    -- Create directory if it doesn't exist
    local dir_path = full_path:match("(.*/)")
    if dir_path then
        local mkdir_result = os.execute("mkdir -p '" .. dir_path .. "'")
        ngx.log(ngx.INFO, "Directory creation: ", dir_path, " result:", mkdir_result or "unknown")
    end
    
    -- Read request body
    ngx.req.read_body()
    local body = ngx.req.get_body_data()
    
    if not body then
        -- Body might be too large, try to get it from file
        local body_file = ngx.req.get_body_file()
        ngx.log(ngx.INFO, "Reading large body from temp file: ", body_file or "none")
        if body_file then
            local file = io.open(body_file, "rb")
            if file then
                body = file:read("*a")
                file:close()
                ngx.log(ngx.INFO, "Read body from temp file: ", #body, " bytes")
            else
                ngx.log(ngx.ERR, "Could not open temp body file: ", body_file)
                ngx.status = 500
                ngx.header["Content-Type"] = "application/json"
                ngx.say('{"error": "Could not read request body"}')
                ngx.exit(500)
            end
        else
            ngx.log(ngx.WARN, "Upload failed - empty body: ", client_ip, " ", file_path)
            ngx.status = 400
            ngx.header["Content-Type"] = "application/json"
            ngx.say('{"error": "Empty request body"}')
            ngx.exit(400)
        end
    else
        ngx.log(ngx.INFO, "Read body directly: ", #body, " bytes")
    end
    
    -- Write file
    ngx.log(ngx.INFO, "Writing file: ", full_path, " (", #body, " bytes)")
    local file = io.open(full_path, "wb")
    if not file then
        ngx.log(ngx.ERR, "Could not create file: ", full_path)
        ngx.status = 500
        ngx.header["Content-Type"] = "application/json"
        ngx.say('{"error": "Could not create file"}')
        ngx.exit(500)
    end
    
    local success, err = file:write(body)
    file:close()
    
    if not success then
        ngx.log(ngx.ERR, "Could not write to file: ", full_path, " error: ", err or "unknown")
        ngx.status = 500
        ngx.header["Content-Type"] = "application/json"
        ngx.say('{"error": "Could not write file"}')
        ngx.exit(500)
    end
    
    ngx.log(ngx.INFO, "File written successfully: ", full_path, " (", #body, " bytes)")
    
    -- Calculate MD5 hash of uploaded file
    local md5_cmd = "md5sum '" .. full_path .. "' | cut -d' ' -f1"
    local handle = io.popen(md5_cmd)
    local md5_hash = ""
    if handle then
        md5_hash = handle:read("*a"):gsub("%s+", "")
        handle:close()
        ngx.log(ngx.INFO, "MD5 calculated: ", md5_hash)
    else
        ngx.log(ngx.WARN, "Could not calculate MD5 for: ", full_path)
    end
    
    -- Set file permissions (readable by nginx)
    local chmod_result = os.execute("chmod 644 '" .. full_path .. "'")
    ngx.log(ngx.INFO, "File permissions set: ", full_path, " result:", chmod_result or "unknown")
    
    -- Calculate upload duration
    local end_time = ngx.now()
    local duration = end_time - start_time
    local rate = #body / duration / 1024 -- KB/s
    
    -- Log successful upload
    ngx.log(ngx.INFO, "Upload completed: ", client_ip, " -> ", file_path, 
            " size:", #body, " duration:", string.format("%.2f", duration), "s",
            " rate:", string.format("%.1f", rate), "KB/s",
            " md5:", md5_hash)
    
    -- Return success response
    ngx.status = 201
    ngx.header["Content-Type"] = "application/json"
    ngx.say('{"status": "success", "path": "' .. file_path .. '", "size": ' .. #body .. ', "md5": "' .. md5_hash .. '", "duration": ' .. string.format("%.2f", duration) .. ', "rate_kbps": ' .. string.format("%.1f", rate) .. '}')
end

handle_upload()