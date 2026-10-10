# Demo cover sources

The international demo uses Sherlock Holmes BBC audio productions. Cover art
remains the property of its respective rights holders. Full-size sources are
local to the Git-ignored `docs/screenshots/source-covers` directory and are not
shipped in app packages. Only native UI previews appear in the repository.

| Local source | Demo title | Publisher cover |
| --- | --- | --- |
| sherlock-1.jpg | The Adventures of Sherlock Holmes | [BBC / Penguin, ISBN 9781785299995](https://cdn.penguin.co.uk/dam-assets/books/9781785299995/9781785299995-jacket-large.jpg) |
| sherlock-2.jpg | The Memoirs of Sherlock Holmes | [BBC / Penguin, ISBN 9781785292095](https://cdn.penguin.co.uk/dam-assets/books/9781785292095/9781785292095-jacket-large.jpg) |
| sherlock-3.jpg | The Return of Sherlock Holmes | [BBC / Penguin, ISBN 9781785298622](https://cdn.penguin.co.uk/dam-assets/books/9781785298622/9781785298622-jacket-large.jpg) |
| sherlock-4.jpg | His Last Bow | [BBC / Penguin, ISBN 9781785296697](https://cdn.penguin.co.uk/dam-assets/books/9781785296697/9781785296697-jacket-large.jpg) |
| sherlock-5.jpg | The Casebook of Sherlock Holmes | [BBC / Penguin, ISBN 9781473531116](https://cdn.penguin.co.uk/dam-assets/books/9781473531116/9781473531116-jacket-large.jpg) |

The replacement sources are square 500 x 500 publisher images. German radio-play,
podcast and music covers remain the existing local sources. Playback progress,
track counts and chapter names are illustrative demo data.

Regenerate both galleries and GIFs:

```powershell
./scripts/Generate-DemoScreenshots.ps1 -Language de -IncludeGif
./scripts/Generate-DemoScreenshots.ps1 -Language en -IncludeGif
```

GIF encoding requires Python and Pillow. The screenshot tool records 190 frames
at 25 fps with the renderer's normal navigation easing. Identical still frames
are merged by the encoder. This is not a handheld frame-rate measurement.
