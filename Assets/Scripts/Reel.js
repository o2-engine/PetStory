ImageInfo = class ImageInfo
{
    regularImage = new o2.AssetRefImageAsset();
    blurredImage = new o2.AssetRefImageAsset();
}

Reel = class Reel extends o2.Component
{
    imagesContainer = new o2.LinkRefActor();
    images = [ new ImageInfo() ];
    imagesDistance = 100; 
    rotationSpeed = 100;
    blurRotationSpeedThreshold = 10;
    beginRotationTime = 0.5;
    endRotationTime = 0.5;

    mRotationImages = [];
    mIsRotating = false;
    mRotatingOffset = 0.0;
    mCurrentRotationSpeed = 0.0;

    constructor()
    {
        super();
    }

    StartRotation()
    {
        this.mIsRotating = true;
    }

    StopRotation()
    {
        this.mIsRotating = false;
    }

    OnStart() 
    {
        this.CreateImages();
    }

    OnDisabled()
    {
        this.DestroyImages();
    }

    Update(dt)
    {
        if (this.mIsRotating)
        {
            if (this.mCurrentRotationSpeed < this.rotationSpeed)
                this.mCurrentRotationSpeed += this.rotationSpeed / this.beginRotationTime * dt;
        }
        else
        {
            if (this.mCurrentRotationSpeed > 0.0)
                this.mCurrentRotationSpeed -= this.rotationSpeed / this.endRotationTime * dt;
        }

        this.mCurrentRotationSpeed = Math.max(0.0, Math.min(this.mCurrentRotationSpeed, this.rotationSpeed));
        this.mRotatingOffset += this.mCurrentRotationSpeed * dt;

        this.UpdateImagesLayout();
    }

    CreateImages()
    {
        if (!this.imagesContainer.Get())
            return;

        //this.DestroyImages();

        this.mRotationImages = [];

        // Shuffle images
        let tmpImages = [...this.images];
        while (tmpImages.length > 0)
        {
            let randomIndex = Math.floor(Math.random() * tmpImages.length);
            this.mRotationImages.push({ info: tmpImages[randomIndex], image: null });
            tmpImages.splice(randomIndex, 1);
        }

        // Create image widgets from shuffled images
        for (let rotatingImage of this.mRotationImages)
        {
            let imageAsset = rotatingImage.info.regularImage;

            let img = new o2.Image();
            img.imageAsset = imageAsset;
            img.SetParent(this.imagesContainer.Get(), false);
            rotatingImage.image = img;
        }

        this.UpdateImagesLayout();
    }

    DestroyImages()
    {
        let container = this.imagesContainer.Get();
        if (!container)
            return;

        let children = container.GetChildren();
        for (let child of children)
            child.Destroy();

        this.mRotationImages = [];
    }

    UpdateImagesLayout()
    {
        let isBlurred = this.mCurrentRotationSpeed > this.blurRotationSpeedThreshold;
        let count = this.mRotationImages.length;
        let allImagesHeight = this.imagesDistance * count;

        for (let i = 0; i < count; i++)
        {
            let rotatingImage = this.mRotationImages[i];

            let rawOffset = (i * this.imagesDistance + this.mRotatingOffset) % allImagesHeight;
            if (rawOffset < 0) rawOffset += allImagesHeight;
            let imageOffset = rawOffset - allImagesHeight / 2.0 + this.imagesDistance / 2.0;

            let asset = isBlurred ? rotatingImage.info.blurredImage : rotatingImage.info.regularImage;
            rotatingImage.image.imageAsset = asset;

            let assetInst = asset.Get();
            if (!assetInst)
                continue;

            let size = assetInst.GetSize();
            if (isBlurred)
            {
                size.x *= 2.0;
                size.y *= 2.0;
            }

            rotatingImage.image.GetLayout().Set(
                o2.WidgetLayout.Based("Center", size, new Vec2(0.0, imageOffset)));
        }
    }
}