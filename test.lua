io.write("\
\
")

headers = {
  ["Content-Type"] = "text/html",
  ["Status"] = "200"
}

function send_headers() end

headers["Content-Type"] = "text/html; charset=utf-8"

local oldtostring = tostring
function tostring(o, fmt)
  local mt = getmetatable(o)
  if type(mt) == "table" and mt.__tostring then
    return mt.__tostring(o, fmt)
  else
    return oldtostring(o)
  end
end

page_title = "Hello World"
items = {
  ["Apples"] = "Fruit",
  ["Evil kitten"] = "Animal"
}


io.write("\
<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\
<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\
  <head>\
    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>\
    <title>")
io.write( page_title )
io.write("</title>\
  </head>\
  ")
io.write("\
  <body>\
    <ul>\
    ")
for k, v in pairs(headers) do 
io.write("\
      <li><b>")
io.write( k )
io.write("</b> = ")
io.write(v)
io.write("</li>\
    ")
end 
io.write("\
    </ul>\
  </body>  \
  ")
io.write("\
</html>\
")
