local function check_auth()
    -- Get API key from header or query parameter
    local api_key = ngx.var.http_x_api_key or ngx.var.arg_key
    local client_ip = ngx.var.remote_addr
    local request_uri = ngx.var.request_uri
    local method = ngx.var.request_method
    
    -- Log authentication attempt
    ngx.log(ngx.INFO, "Auth attempt: ", client_ip, " ", method, " ", request_uri, 
            " key:", api_key and string.sub(api_key, 1, 8) .. "..." or "none")
    
    if not api_key then
        ngx.log(ngx.WARN, "Auth failed - missing API key: ", client_ip, " ", method, " ", request_uri)
        ngx.status = 401
        ngx.header["WWW-Authenticate"] = 'Bearer realm="CDN"'
        ngx.say('{"error": "Missing API key"}')
        ngx.exit(401)
    end
    
    -- Read valid keys
    local file = io.open("/cdn/keys/valid_keys.txt", "r")
    if not file then
        ngx.log(ngx.ERR, "Could not open keys file")
        ngx.status = 500
        ngx.say('{"error": "Internal server error"}')
        ngx.exit(500)
    end
    
    -- Check if key is valid
    local valid = false
    for line in file:lines() do
        local key = line:match("^%s*(.-)%s*$")  -- trim whitespace
        if key ~= "" and key == api_key then
            valid = true
            break
        end
    end
    file:close()
    
    if not valid then
        ngx.log(ngx.WARN, "Auth failed - invalid API key: ", client_ip, " ", method, " ", request_uri, 
                " key:", string.sub(api_key, 1, 8) .. "...")
        ngx.status = 403
        ngx.say('{"error": "Invalid API key"}')
        ngx.exit(403)
    end
    
    -- Log successful authentication
    ngx.log(ngx.INFO, "Auth success: ", client_ip, " ", method, " ", request_uri, 
            " key:", string.sub(api_key, 1, 8) .. "...")
end

check_auth()