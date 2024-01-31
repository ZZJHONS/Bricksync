BrickSync
=========

**Inventory Synchronization Software between BrickLink and BrickOwl**

Contents:
---------

1.  [Questions](#faq)
2.  [Answers](#answers)

### Home Page

This is a collection of questions and answers that come up regularly regarding BrickSync.

If you have a question or problem that isn't listed, [contact me](#21). If you think you may be facing some bug or problem, **please include the relevant log file**. You can find all the log files under the directory _data/logs/_.

Many features are not covered by this FAQ. See the [Command List](COMMAND_LIST.md) for detailed information.

Frequently Asked Questions <a id="faq"></a>
--------------------------

1.  **[What are API keys? Where do I get these?](#1)**

2. **[How do I add inventory?](#2)**

3. **[How does the blmaster mode work?](#3)**

4. **[Can I turn off the computer?](#4)**

5. **[Heeelllp! BrickLink is throwing me BAD\_OAUTH\_REQUEST, SIGNATURE\_INVALID and/or TOKEN\_IP\_MISMATCHED errors!](#4)**

6. **[Do I need BrickStore/BrickStock?](#6)**

7.  **[When synchronizing, what does "Omit X items in X lots due to BLIDs unknown to BrickOwl." mean?](#7)**

8.  **[What can I do about unknown BLIDs?](#9)**

9.  **[How do I update BrickSync to the latest version?](#9)**

10. **[How do I make BrickSync retain empty BrickLink lots?](#10)**

11. **[How can I update my prices?](#11)**

12. **[What is a _sync_ operation? What triggers a deep synchronization?](#12)**

13.  **[How can I make some items available only on BrickOwl or BrickLink?](#13)**

14.  **[Does BrickSync synchronize what's in the BrickLink stockrooms?](#14)**

15.  **[I want to edit just a single item's quantity, remarks or such, what now?](#15)**

16.  **[How do I quickly locate an item for the various edit commands?](#16)**

17.  **[What about cancelled orders?](#17)**

18.  **[I want to stay in blmaster mode permanently. Why not?](#18)**

19.  **[Can I trigger an immediate order check without typing _check_?](#19)**

20.  **[How do I use BrickOwl's various grades of _used_ condition?](#20)**

21.  **[How do I contact the author?](#21)**

Answers  <a id="answers"></a>
-------

1.  **What are API keys? Where do I get these?** <a id="1"></a>

You can obtain your BrickLink API keys from the [BrickLink API](http://api.bricklink.com/pages/api/welcome.page) page. Unless you know your IP address is static, I suggest to set both the "allowed IP" and "mask IP" to 0.0.0.0

You can obtain your BrickOwl API keys from your [Profile page](https://www.brickowl.com/user), under the API Keys tab. Make sure to give the API key permission to view/edit your orders and inventory.

BrickSync requires API keys to fetch orders and edit your inventory on both marketplaces.

2**How should I add new inventory?** <a id="2"></a>

You can use the [_blmaster_](./COMMAND_LIST.md#cmdblmaster) mode, or the _add_ or _merge_ commands. See the [Command List](./COMMAND_LIST.md) for more information.

3**How does the blmaster mode work?** <a id="3"></a>

It allows modifying your store inventory through the BrickLink interface, either web or XML. While the _blmaster_ mode is active, all order processing is suspended.

Step #1: Type _blmaster on_ in BrickSync  
Step #2: Modify your inventory on BrickLink as much as your heart desires  
Step #3: Type _blmaster off_ in BrickSync, and everything is synchronized

4**Can I turn off the computer?**  <a id="4"></a>

You can close BrickSync, turn off the computer and resumes execution later. If you do that, note that orders will not be synchronized until you run BrickSync again. This is not generally a good idea due to the risk of selling the same items on both marketplaces.

You must not allow the computer to sleep or hibernate, it needs to be awake and active to process orders. If the computer goes to sleep, BrickSync's watchdog thread will panic and print errors, as it will see BrickSync has stopped responding for way too long.

5.  **Heeelllp! BrickLink is throwing me BAD\_OAUTH\_REQUEST, SIGNATURE\_INVALID and/or TOKEN\_IP\_MISMATCHED errors!**  <a id="5"></a>

You probably created a BrickLink API key that was restricted to a IP submask, and then your IP address changed. Create a BrickLink API key with an "allowed IP" and "mask IP" both set to 0.0.0.0

If it's your first time running BrickSync, also make sure you copied the 4 API values in the proper variables of the configuration file (data/bricksync.conf.txt)

Lastly, if BrickLink is currently going through its daily/monthly maintenance, you will probably see a bunch of API errors. Operations will resume normally when BrickLink comes out of maintenance.

6.  **Do I need BrickStore/BrickStock?**  <a id="6"></a>

You don't _need_ BrickStore/BrickStock to use BrickSync. There are many useful commands working with BrickStore/BrickStock BSX files though. All backups and orders are also stored as BSX files.

I'm developing and using my own inventory management software, also able to read BSX files, but it's not yet ready to go public.

7.  **When synchronizing, what does "Omit X items in X lots due to BLIDs unknown to BrickOwl." mean?**  <a id="7"></a>

BrickSync failed to translate some BrickLink IDs into BrickOwl IDs, and these items won't be uploaded to BrickOwl. See the following question.

8.  **What can I do about unknown BLIDs?**  <a id="8"></a>

The _owlresolve_ command will print the complete list of unknown BLIDs. The unknown BLIDs can be submitted for the matching items in the BrickOwl catalog (under the _Edit_ tab), BrickSync will be able to resolve the BLIDs once they are approved (type _owlresolve_ to update). Alternatively, you can use the _owlsubmitblid_ command to submit a BLID to a BOID in the BrickOwl catalog.

If you don't want to wait for catalog submissions to be approved, the command _owlforceblid_ can immediately create a BLID-BOID match in your BrickSync local translation cache. You can then upload any missing item right away, with the command _sync brickowl_.

9.  **How do I update BrickSync to the latest version?**  <a id="9"></a>

To update BrickSync, you just need to replace your existing executable file (the .exe on Windows) with the one found in the latest BrickSync package. Don't replace any other file, especially keep your existing _data/_ subdirectory intact.

After replacing the executable, simply launch BrickSync again. Note that BrickSync will _always_ be able to read files generated by previous versions.

10.  **How do I make BrickSync retain empty BrickLink lots?**  <a id="10"></a>

BrickSync does not retain empty lots by default. To change this behavior, you need to change the configuration variable _retainemptylots_ in the file _data/bricksync.conf.txt_:  
_retainemptylots = 1;_

If you retain BrickLink lots, you might want to reuse matching BrickOwl lots as well. This is enabled through the _brickowl.reuseempty_ configuration variable:  
_brickowl.reuseempty = 1;_

After modifying _data/bricksync.conf.txt_, remember that you need to launch BrickSync again to read your new configuration.  
You can type _help conf_ to print your current configuration. If variables are defined multiple times, only the last assignment takes effect.

11.  **How can I update my prices?**  <a id="11"></a>

You can use the [blmaster](./COMMAND_LIST.md#cmdblmaster) mode and update them on BrickLink. Another convenient option is to update all prices for matching lots from a BSX file. Consult the Command List page for the [loadprices](./COMMAND_LIST.md#cmdloadprices) command, there's also _loadnotes_, _loadmycost_ and _loadall_.

Note that prices are synchronized but sale percentages are not, so that you can run different sales on each marketplace.

12.  **What is a _sync_ operation? What triggers a deep synchronization?**  <a id="12"></a>

A deep synchronization is an operation performed by BrickSync to make sure a remote service (BrickLink, BrickOwl or both) matches exactly the inventory being tracked locally by BrickSync. The slightest difference will be corrected to ensure a perfect match.

The operation can be manually triggered by typing _sync_. For example, BrickSync may suggest to run a _sync_ operation after a _owlresolve_ command, to upload the newly resolved items to BrickOwl. The _verify_ command also verifies synchronization but without performing the required changes.

BrickSync may also automatically perform a deep synchronization, typically in order to detect and fix any inventory mismatch. This typically occurs when:  
\- A networking error occurs, and BrickSync doesn't know if an update was received and processed by the server  
\- An important inventory update fails, such as adjusting a lot's quantity

13.  **How can I make some items available only on BrickOwl or BrickLink?**  <a id="13"></a>

You need to _tag_ these items: insert the **~** character anywhere in the remarks (private notes) of these items, and BrickSync won't see them.

14.  **Does BrickSync synchronize what's in the BrickLink stockrooms?**  <a id="14"></a>

No, BrickSync does not see or synchronize the content of the BrickLink stockrooms.

15.  **I want to edit just a single item's quantity, remarks or such, what now?**  <a id="15"></a>

Please don't use the _blmaster_ mode for this, it's overkill and consumes a lot of bandwidth. Instead, you can use commands such as _setquantity_, _setremarks_ and so on. These commands require an item identifier, see the following question. Also see the [Command List](./COMMAND_LIST.md) for more information.

16.  **How can I quickly locate or identify an item?**  <a id="16"></a>

The _find_ command searches through your inventory for items that match all the terms specified. It searches through all fields: name, color, remarks, quantity and so on. From the search results, you can use the LotID as argument for commands such as _setquantity_, which adjusts a lot's quantity. Examples:  
_find "Brick 2 x 4" Black_  
_find 3001 "luish Gray"_  
_find B36 Red_  
This is easily faster than searching your inventory by other means. There's no picture though.

17.  **What about cancelled orders?**  <a id="17"></a>

BrikcSync does not put the inventory back for sale for cancelled orders. The correction has to be done manually. On BrickLink, you could use the _blmaster_ mode while adding the items back. The _add_ command can also be used to put the items back in stock, using the BSX file that was saved for the cancelled order. Examples:  
_add data/orders/brickowl-12345678.bsx_  
_add data/orders/bricklink-12345678.bsx_  
If you are confused about if items were added back twice, you can use _sync_ to make sure both BrickLink and BrickOwl match the inventory tracked by BrickSync.

18.  **I want to stay in blmaster mode permanently. Why not?**  <a id="18"></a>

BrickSync always tracks your store's true inventory, it knows how your inventory should be exactly in every detail. If an update to BrickLink or BrickOwl fails with ambiguous results, such as the connection being closed without receiving a reply from the server, we can not know if the update was received and processed by the server. Therefore, BrickSync must connect back, fetch the inventory and apply any correction necessary (a _sync_ operation)

If it were possible to edit the BrickLink inventory just when BrickSync needs to synchronize after failed updates, it would be ambiguous if a BrickLink inventory change came from an external source or from a failed API update. Therefore, for maximum robustness, BrickSync must stop processing orders when the _blmaster_ is active. Due to the design of BrickLink's API, there is no other way.

19.  **Can I trigger an immediate order check without typing _check_?**  <a id="19"></a>

Yes. If you use email monitoring software and want BrickSync to process orders immediately when receiving an email, you can send the SIGUSR1 signal to the bricksync process (Linux and OSX only). There's no equivalent mechanism available on Windows.

This is not generally required, the software polls BrickLink and BrickOwl for new orders every couple minutes (unless _autocheck_ is disabled). You can configure the polling intervals in the configuration file.

20.  **How do I use BrickOwl's various grades of _used_ condition?**  <a id="20"></a>

When uploading items in _used_ condition, BrickSync uses a home-made natural language parser to read a lot's comments (public notes) and estimate its quality grade. It can understand compound statements and some 200 words of English. If you have recently updated some comments of your inventory and would like BrickSync to regrade lots in _used_ condition, use the _regradeused_ command. Some examples of comments and the grade given by the parser:  
_No large scratches_   (good)  
_Few tiny scratches_   (good)  
_Many tiny scratches_   (acceptable)  
_Absolutely mint_   (like new)  
_Slightly faded_   (acceptable)  
_Not quite new_   (good)  
_starting to rub off_   (acceptable)  
_Heavy play wear_   (acceptable)  
_Chewed on_   (acceptable)  
_works perfectly_   (like new)  
_Excellent condition, but the glass is missing_   (good)

12.  **How do I contact the author?**  <a id="21"></a>

Contact
-------

*   [Stragus on BrickOwl](http://www.brickowl.com/user/9775/contact)
*   [Stragus on BrickLink](http://www.bricklink.com/contact.asp?u=Stragus)

Special Thanks
--------------
Special thanks to BrickFeverParts, budgetkids, DadsAFOL, playonbricks, PSBricks and many others for the beta version testing and support!

*   [BrickFeverParts](http://brickfeverparts.brickowl.com/) for being the biggest supporter of BrickSync!
*   [budgetkids](http://budgetkids.brickowl.com/) for the beta testing, numerous bug reports and support!
*   [DadsAFOL](http://constructibles.brickowl.com/) for the bug reports and being the very first supporter of BrickSync!
*   [BadLove](http://badlovesbricks.brickowl.com/) for the recurrent and well appreciated support!
*   [PSBricks/Bugsy](http://brickstopde.brickowl.com/) for the beta testing and the many useful bug reports!
*   [Brick Machine Shop](http://www.bricklink.com/store.asp?p=Eezo) for their fantastic aluminum Technic parts! (Okay, that's totally unrelated to BrickSync, but I love their products :) )
*   Many others who have supported the software through their donations, thanks all!
