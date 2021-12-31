# sro_loader
A Multi-Client loader for [Silkroad Online].

[Silkroad Online]: https://www.silkroadonline.net/

## Features
* Simple, Run multiple clients with a single click.
* GatewayServer connection redirection.
* Should work (_Theoretically_) with every version of Silkroad.
* Tested on VSRO/CSRO/ISRO.
 
## Limitation
* Does not bypass the IP Limit (2 connections/IP) a Proxy/VPN is needed for that.

## Usage as a simple user (Normal Player)
* Go to the [Releases Page].
* Download the latest version available.
* Extract the ZIP file is the same directory as Silkroad Online.
* Run `sro_loader.cmd` to start a new client.
* Have fun!

[Releases Page]: https://github.com/halimsamy/sro_loader/releases

## Usage
```powershell
sro_loader.exe silkroad_game_directory loader_dll_directory [redirect]
```
### Example
Without Gateway redirection:
```powershell
sro_loader.exe C:\Silkroad .\sro_loader.dll
```
With Gateway redirection:
```powershell
sro_loader.exe C:\Silkroad .\sro_loader.dll redirect
```

## Gateway redirection
The Gateway connection will be redirected to the localhost `127.0.0.1` and the port will be the same as the started `sro_client.exe` process ID.

## License
This project is licensed under the MIT license.

### Contribution
Unless you explicitly state otherwise, any contribution intentionally submitted
by you, shall be licensed as MIT, without any additional terms or conditions.
