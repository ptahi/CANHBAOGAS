const char* configForm = R"html(
<!DOCTYPE HTML>
<html>
<head>
  <title>WiFi setup</title>
  <style>
  body {
    background-color: #fcfcfc;
    box-sizing: border-box;
  }
  body, input {
    font-family: Roboto, sans-serif;
    font-weight: 400;
    font-size: 16px;
  }
  .centered {
    position: fixed;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);

    padding: 20px;
    background-color: #ccc;
    border-radius: 4px;
  }
  td { padding:0 0 0 5px; }
  label { white-space:nowrap; }
  input { width: 17em; }
  input[name="port"] { width: 5em; }
  input[type="submit"], img { margin: auto; display: block; width: 30%; }
  </style>
  <meta name="viewport" content="width=device-width,user-scalable=0" charset="utf-8">
</head> 
<body>
<div class="centered">
  <form method="get" action="configsave">
    <table>
    <tr><td><label for="ssid_sta">WiFi SSID:</label></td>  <td><input id="ssid_sta" type="text" name="ssid_sta" length=64 required="required"></td></tr>
    <tr>
      <td></td>
      <td>
        <select id="ssid_list" onchange="updatessid_sta()">
          <option>Choose a wifi network</option>
        </select>
      </td>
    </tr>
    <tr><td><label for="pass_sta">Password:</label></td>   <td><input type="text" name="pass_sta" length=64></td></tr>
    <tr><td><label for="blynk_auth">Auth token:</label></td><td><input type="text" name="blynk_auth" placeholder="a0b1c2d..." pattern="[-_a-zA-Z0-9]{32}" maxlength="32" required="required"></td></tr>
    </table><br/>
    <input type="submit" value="Apply">
  </form>
</div>
<script type="text/javascript">
  window.onload = function(){
    getWifiList();   
  }
  var xhttp = new XMLHttpRequest();
  function getWifiList(){
    xhttp.onreadystatechange = function(){
      if(xhttp.readyState==4 && xhttp.status==200){
        var obj = JSON.parse(xhttp.responseText);
        var ssidlist = obj;
        var select = document.getElementById("ssid_list");
        for(var i=0; i<ssidlist.length;++i){
          select[select.length] = new Option(ssidlist[i],ssidlist[i]);
        }
      }
    }
    xhttp.open("GET","/wifiscan.json",true);
    xhttp.send();
  }
  function updatessid_sta(){
    document.getElementById("ssid_sta").value = document.getElementById("ssid_list").value;
  }
</script>
</body>
</html>
)html";